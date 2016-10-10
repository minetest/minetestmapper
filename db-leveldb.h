#ifndef DB_LEVELDB_HEADER
#define DB_LEVELDB_HEADER

#include "db.h"
#include <leveldb/db.h>

class DBLevelDB : public DB {
public:
	DBLevelDB(const std::string &mapdir);
	virtual std::vector<BlockPos> getBlockPos();
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos);
	virtual ~DBLevelDB();
private:
	void loadPosCache();

	std::vector<BlockPos> posCache;

	leveldb::DB *db;
};

#endif // DB_LEVELDB_HEADER
