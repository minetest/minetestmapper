#include <stdexcept>
#include <sstream>
#include <fstream>
#include "db-redis.h"
#include "types.h"
#include "util.h"

static inline int64_t stoi64(const std::string &s)
{
	std::stringstream tmp(s);
	int64_t t;
	tmp >> t;
	return t;
}


static inline std::string i64tos(int64_t i)
{
	std::ostringstream os;
	os << i;
	return os.str();
}

std::string get_setting_default(std::string name, std::istream &is, const std::string def)
{
	try {
		return get_setting(name, is);
	} catch(std::runtime_error e) {
		return def;
	}
}

DBRedis::DBRedis(const std::string &mapdir)
{
	std::ifstream ifs((mapdir + "/world.mt").c_str());
	if(!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string tmp;
	try {
		tmp = get_setting("redis_address", ifs);
		ifs.seekg(0);
		hash = get_setting("redis_hash", ifs);
		ifs.seekg(0);
	} catch(std::runtime_error e) {
		throw std::runtime_error("Set redis_address and redis_hash in world.mt to use the redis backend");
	}
	const char *addr = tmp.c_str();
	int port = stoi64(get_setting_default("redis_port", ifs, "6379"));
	ctx = redisConnect(addr, port);
	if(!ctx)
		throw std::runtime_error("Cannot allocate redis context");
	else if(ctx->err) {
		std::string err = std::string("Connection error: ") + ctx->errstr;
		redisFree(ctx);
		throw std::runtime_error(err);
	}

	loadPosCache();
}


DBRedis::~DBRedis()
{
	redisFree(ctx);
}


std::vector<BlockPos> DBRedis::getBlockPos()
{
	return posCache;
}


std::string DBRedis::replyTypeStr(int type) {
	switch(type) {
		case REDIS_REPLY_STATUS:
			return "REDIS_REPLY_STATUS";
		case REDIS_REPLY_ERROR:
			return "REDIS_REPLY_ERROR";
		case REDIS_REPLY_INTEGER:
			return "REDIS_REPLY_INTEGER";
		case REDIS_REPLY_NIL:
			return "REDIS_REPLY_NIL";
		case REDIS_REPLY_STRING:
			return "REDIS_REPLY_STRING";
		case REDIS_REPLY_ARRAY:
			return "REDIS_REPLY_ARRAY";
		default:
			return "UNKNOWN_REPLY_TYPE";
	}
}


void DBRedis::loadPosCache()
{
	redisReply *reply;
	reply = (redisReply*) redisCommand(ctx, "HKEYS %s", hash.c_str());
	if(!reply)
		throw std::runtime_error(std::string("redis command 'HKEYS %s' failed: ") + ctx->errstr);
	if(reply->type != REDIS_REPLY_ARRAY)
		throw std::runtime_error("Failed to get keys from database");
	for(size_t i = 0; i < reply->elements; i++) {
		if(reply->element[i]->type != REDIS_REPLY_STRING)
			throw std::runtime_error("Got wrong response to 'HKEYS %s' command");
		posCache.push_back(decodeBlockPos(stoi64(reply->element[i]->str)));
	}

	freeReplyObject(reply);
}


void DBRedis::getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos)
{
	redisReply *reply;
	std::string tmp;

	for (std::vector<BlockPos>::iterator it = posCache.begin(); it != posCache.end(); ++it) {
		if (it->z != zPos) {
			continue;
		}
		tmp = i64tos(encodeBlockPos(*it));
		reply = (redisReply*) redisCommand(ctx, "HGET %s %s", hash.c_str(), tmp.c_str());
		if(!reply)
			throw std::runtime_error(std::string("redis command 'HGET %s %s' failed: ") + ctx->errstr);
		if (reply->type == REDIS_REPLY_STRING && reply->len != 0) {
			Block b(*it, ustring((const unsigned char *) reply->str, reply->len));
			blocks[b.first.x].push_back(b);
		} else
			throw std::runtime_error("Got wrong response to 'HGET %s %s' command");
		freeReplyObject(reply);
	}
}
