#pragma once

#include "db.h"
#include <unordered_map>
#include <utility>
#include <functional>
#include <hiredis/hiredis.h>

class DBRedis : public DB {
public:
	DBRedis(const std::string &mapdir);
	std::vector<BlockPos> getBlockPos(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBRedis() override;

	bool preferRangeQueries() const override { return false; }

private:
	using pos2d = std::pair<int16_t, int16_t>;
	static const char *replyTypeStr(int type);

	void loadPosCache();
	void HMGET(const std::vector<BlockPos> &positions,
			std::function<void(std::size_t, ustring)> result);

	// indexed by Z, contains all (x,y) position pairs
	std::unordered_map<int16_t, std::vector<pos2d>> posCache;

	redisContext *ctx;
	std::string hash;
};
