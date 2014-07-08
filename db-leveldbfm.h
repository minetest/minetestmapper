#ifndef DB_LEVELDBFM_HEADER
#define DB_LEVELDBFM_HEADER

#include "db-leveldb.h"
#include <leveldb/db.h>

class DBLevelDBFM : public DBLevelDB {
public:
	DBLevelDBFM(const std::string &mapdir);
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos);
	~DBLevelDBFM();

protected:
	BlockPos getStringAsBlock(const std::string &i);
	virtual void loadPosCache();
};

#endif // DB_LEVELDBFM_HEADER
