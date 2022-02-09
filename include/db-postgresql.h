#pragma once

#include "db.h"
#include <libpq-fe.h>

class DBPostgreSQL : public DB {
public:
	DBPostgreSQL(const std::string &mapdir);
	std::vector<BlockPos> getBlockPos(BlockPos min, BlockPos max) override;
	void getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y) override;
	void getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions) override;
	~DBPostgreSQL() override;

	bool preferRangeQueries() const override { return true; }

protected:
	PGresult *checkResults(PGresult *res, bool clear = true);
	void prepareStatement(const std::string &name, const std::string &sql);
	PGresult *execPrepared(
		const char *stmtName, const int paramsNumber,
		const void **params,
		const int *paramsLengths = nullptr, const int *paramsFormats = nullptr,
		bool clear = true
	);
	int pg_binary_to_int(PGresult *res, int row, int col);
	BlockPos pg_to_blockpos(PGresult *res, int row, int col);

private:
	PGconn *db;
};
