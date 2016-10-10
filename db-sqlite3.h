#ifndef _DB_SQLITE3_H
#define _DB_SQLITE3_H

#include "db.h"
#include <sqlite3.h>

class DBSQLite3 : public DB {
public:
	DBSQLite3(const std::string &mapdir);
	virtual std::vector<BlockPos> getBlockPos();
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos);
	virtual ~DBSQLite3();
private:
	sqlite3 *db;

	sqlite3_stmt *stmt_get_block_pos;
	sqlite3_stmt *stmt_get_blocks_z;
};

#endif // _DB_SQLITE3_H
