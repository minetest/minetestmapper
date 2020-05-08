#pragma once

#include "db.h"
#include <unordered_map>
#include <utility>
#include <leveldb/db.h>

class DBLevelDB : public DB {
public:
	DBLevelDB(const std::string &mapdir);
	std::vector<BlockPos> getBlockPos(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
			int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBLevelDB() override;

	bool preferRangeQueries() const override { return false; }

private:
	using pos2d = std::pair<int16_t, int16_t>;

	void loadPosCache();

	// indexed by Z, contains all (x,y) position pairs
	std::unordered_map<int16_t, std::vector<pos2d>> posCache;
	leveldb::DB *db;
};
