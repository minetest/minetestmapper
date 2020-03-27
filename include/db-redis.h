#ifndef DB_REDIS_HEADER
#define DB_REDIS_HEADER

#include "db.h"
#include <unordered_map>
#include <utility>
#include <hiredis/hiredis.h>

class DBRedis : public DB {
public:
	DBRedis(const std::string &mapdir);
	std::vector<BlockPos> getBlockPos(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y) override;
	~DBRedis() override;

private:
	using pos2d = std::pair<int16_t, int16_t>;
	static std::string replyTypeStr(int type);

	void loadPosCache();
	void HMGET(const std::vector<BlockPos> &positions, std::vector<ustring> *result);

	// indexed by Z, contains all (x,y) position pairs
	std::unordered_map<int16_t, std::vector<pos2d>> posCache;

	redisContext *ctx;
	std::string hash;
};

#endif // DB_REDIS_HEADER
