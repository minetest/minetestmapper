#ifndef DB_REDIS_HEADER
#define DB_REDIS_HEADER

#include "db.h"
#include <hiredis.h>

class DBRedis : public DB {
public:
	DBRedis(const std::string &mapdir);
	virtual std::vector<BlockPos> getBlockPos();
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos);
	virtual ~DBRedis();
private:
	static std::string replyTypeStr(int type);

	void loadPosCache();
	void HMGET(const std::vector<BlockPos> &positions, std::vector<ustring> *result);

	std::vector<BlockPos> posCache;

	redisContext *ctx;
	std::string hash;
};

#endif // DB_REDIS_HEADER
