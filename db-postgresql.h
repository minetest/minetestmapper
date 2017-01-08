#ifndef _DB_POSTGRESQL_H
#define _DB_POSTGRESQL_H

#include "db.h"
#include <libpq-fe.h>

class DBPostgreSQL : public DB {
public:
	DBPostgreSQL(const std::string &mapdir);
	virtual std::vector<BlockPos> getBlockPos();
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos);
	virtual ~DBPostgreSQL();
protected:
	PGresult *checkResults(PGresult *res, bool clear = true);
	void prepareStatement(const std::string &name, const std::string &sql);
	PGresult *execPrepared(
		const char *stmtName, const int paramsNumber,
		const void **params,
		const int *paramsLengths = NULL, const int *paramsFormats = NULL,
		bool clear = true, bool nobinary = true
	);
	int pg_to_int(PGresult *res, int row, int col);
	int pg_binary_to_int(PGresult *res, int row, int col);
	BlockPos pg_to_blockpos(PGresult *res, int row, int col);
private:
	PGconn *db;
};

#endif // _DB_POSTGRESQL_H
