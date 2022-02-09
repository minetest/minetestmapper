#include <stdexcept>
#include <sstream>
#include <fstream>
#include "db-redis.h"
#include "types.h"
#include "util.h"

#define DB_REDIS_HMGET_NUMFIELDS 30

#define REPLY_TYPE_ERR(reply, desc) do { \
	throw std::runtime_error(std::string("Unexpected type for " desc ": ") \
			+ replyTypeStr((reply)->type)); \
	} while(0)

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


DBRedis::DBRedis(const std::string &mapdir)
{
	std::ifstream ifs(mapdir + "world.mt");
	if (!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string tmp;

	tmp = read_setting("redis_address", ifs);
	ifs.seekg(0);
	hash = read_setting("redis_hash", ifs);
	ifs.seekg(0);

	if (tmp.find('/') != std::string::npos) {
		ctx = redisConnectUnix(tmp.c_str());
	} else {
		int port = stoi64(read_setting_default("redis_port", ifs, "6379"));
		ctx = redisConnect(tmp.c_str(), port);
	}

	if (!ctx) {
		throw std::runtime_error("Cannot allocate redis context");
	} else if (ctx->err) {
		std::string err = std::string("Connection error: ") + ctx->errstr;
		redisFree(ctx);
		throw std::runtime_error(err);
	}

	/* Redis is just a key-value store, so the only optimization we can do
	 * is to cache the block positions that exist in the db.
	 */
	loadPosCache();
}


DBRedis::~DBRedis()
{
	redisFree(ctx);
}


std::vector<BlockPos> DBRedis::getBlockPos(BlockPos min, BlockPos max)
{
	std::vector<BlockPos> res;
	for (const auto &it : posCache) {
		if (it.first < min.z || it.first >= max.z)
			continue;
		for (auto pos2 : it.second) {
			if (pos2.first < min.x || pos2.first >= max.x)
				continue;
			if (pos2.second < min.y || pos2.second >= max.y)
				continue;
			res.emplace_back(pos2.first, pos2.second, it.first);
		}
	}
	return res;
}


const char *DBRedis::replyTypeStr(int type)
{
	switch (type) {
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
			return "(unknown)";
	}
}


void DBRedis::loadPosCache()
{
	redisReply *reply;
	reply = (redisReply*) redisCommand(ctx, "HKEYS %s", hash.c_str());
	if (!reply)
		throw std::runtime_error("Redis command HKEYS failed");
	if (reply->type != REDIS_REPLY_ARRAY)
		REPLY_TYPE_ERR(reply, "HKEYS reply");
	for (size_t i = 0; i < reply->elements; i++) {
		if (reply->element[i]->type != REDIS_REPLY_STRING)
			REPLY_TYPE_ERR(reply->element[i], "HKEYS subreply");
		BlockPos pos = decodeBlockPos(stoi64(reply->element[i]->str));
		posCache[pos.z].emplace_back(pos.x, pos.y);
	}

	freeReplyObject(reply);
}


void DBRedis::HMGET(const std::vector<BlockPos> &positions,
	std::function<void(std::size_t, ustring)> result)
{
	const char *argv[DB_REDIS_HMGET_NUMFIELDS + 2];
	argv[0] = "HMGET";
	argv[1] = hash.c_str();

	auto position = positions.begin();
	size_t remaining = positions.size();
	size_t abs_i = 0;
	while (remaining > 0) {
		const size_t batch_size = mymin<size_t>(DB_REDIS_HMGET_NUMFIELDS, remaining);

		redisReply *reply;
		{
			// storage to preserve validity of .c_str()
			std::string keys[batch_size];
			for (size_t i = 0; i < batch_size; ++i) {
				keys[i] = i64tos(encodeBlockPos(*position++));
				argv[i+2] = keys[i].c_str();
			}
			reply = (redisReply*) redisCommandArgv(ctx, batch_size + 2, argv, NULL);
		}

		if (!reply)
			throw std::runtime_error("Redis command HMGET failed");
		if (reply->type != REDIS_REPLY_ARRAY)
			REPLY_TYPE_ERR(reply, "HMGET reply");
		if (reply->elements != batch_size) {
			freeReplyObject(reply);
			throw std::runtime_error("HMGET wrong number of elements");
		}
		for (size_t i = 0; i < reply->elements; ++i) {
			redisReply *subreply = reply->element[i];
			if (subreply->type == REDIS_REPLY_NIL)
				continue;
			else if (subreply->type != REDIS_REPLY_STRING)
				REPLY_TYPE_ERR(subreply, "HMGET subreply");
			if (subreply->len == 0)
				throw std::runtime_error("HMGET empty string");
			result(abs_i + i, ustring(
				reinterpret_cast<const unsigned char*>(subreply->str),
				subreply->len
			));
		}
		freeReplyObject(reply);

		abs_i += batch_size;
		remaining -= batch_size;
	}
}


void DBRedis::getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y)
{
	auto it = posCache.find(z);
	if (it == posCache.cend())
		return;

	std::vector<BlockPos> positions;
	for (auto pos2 : it->second) {
		if (pos2.first == x && pos2.second >= min_y && pos2.second < max_y)
			positions.emplace_back(x, pos2.second, z);
	}

	getBlocksByPos(blocks, positions);
}


void DBRedis::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	auto result = [&] (std::size_t i, ustring data) {
		blocks.emplace_back(positions[i], std::move(data));
	};
	HMGET(positions, result);
}
