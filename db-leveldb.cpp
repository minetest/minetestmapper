#include <stdexcept>
#include <sstream>
#include "db-leveldb.h"
#include "types.h"

static inline int64_t stoi64(const std::string &s)
{
	std::istringstream tmp(s);
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

	/* LevelDB is a dumb key-value store, so the only optimization we can do
	 * is to cache the block positions that exist in the db.
	 */
	loadPosCache();
}


DBLevelDB::~DBLevelDB()
{
	delete db;
}


std::vector<BlockPos> DBLevelDB::getBlockPos(BlockPos min, BlockPos max)
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


void DBLevelDB::loadPosCache()
{
	leveldb::Iterator * it = db->NewIterator(leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		int64_t posHash = stoi64(it->key().ToString());
		BlockPos pos = decodeBlockPos(posHash);

		posCache[pos.z].emplace_back(pos.x, pos.y);
	}
	delete it;
}


void DBLevelDB::getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y)
{
	std::string datastr;
	leveldb::Status status;

	auto it = posCache.find(z);
	if (it == posCache.cend())
		return;
	for (auto pos2 : it->second) {
		if (pos2.first != x)
			continue;
		if (pos2.second < min_y || pos2.second >= max_y)
			continue;

		BlockPos pos(x, pos2.second, z);
		status = db->Get(leveldb::ReadOptions(), i64tos(encodeBlockPos(pos)), &datastr);
		if (status.ok()) {
			blocks.emplace_back(
				pos, ustring((unsigned char *) datastr.data(), datastr.size())
			);
		}
	}
}

void DBLevelDB::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	std::string datastr;
	leveldb::Status status;

	for (auto pos : positions) {
		status = db->Get(leveldb::ReadOptions(), i64tos(encodeBlockPos(pos)), &datastr);
		if (status.ok()) {
			blocks.emplace_back(
				pos, ustring((unsigned char *) datastr.data(), datastr.size())
			);
		}
	}
}
