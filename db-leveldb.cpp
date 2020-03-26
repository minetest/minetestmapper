#include <stdexcept>
#include <sstream>
#include "db-leveldb.h"
#include "types.h"

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

DBLevelDB::DBLevelDB(const std::string &mapdir)
{
	leveldb::Options options;
	options.create_if_missing = false;
	leveldb::Status status = leveldb::DB::Open(options, mapdir + "map.db", &db);
	if (!status.ok()) {
		throw std::runtime_error(std::string("Failed to open Database: ") + status.ToString());
	}

	loadPosCache();
}


DBLevelDB::~DBLevelDB()
{
	delete db;
}


std::vector<BlockPos> DBLevelDB::getBlockPos()
{
	return posCache;
}


void DBLevelDB::loadPosCache()
{
	leveldb::Iterator * it = db->NewIterator(leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		int64_t posHash = stoi64(it->key().ToString());
		posCache.push_back(decodeBlockPos(posHash));
	}
	delete it;
}


void DBLevelDB::getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos)
{
	std::string datastr;
	leveldb::Status status;

	for (const auto &it : posCache) {
		if (it.z != zPos) {
			continue;
		}
		status = db->Get(leveldb::ReadOptions(), i64tos(encodeBlockPos(it)), &datastr);
		if (status.ok()) {
			Block b(it, ustring((const unsigned char *) datastr.data(), datastr.size()));
			blocks[b.first.x].push_back(b);
		}
	}
}

