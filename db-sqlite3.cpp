#include <stdexcept>
#include <unistd.h> // for usleep
#include <iostream>
#include "db-sqlite3.h"
#include "types.h"

#define SQLRES(f, good) \
	result = (sqlite3_##f);\
	if (result != good) {\
		throw std::runtime_error(sqlite3_errmsg(db));\
	}
#define SQLOK(f) SQLRES(f, SQLITE_OK)


DBSQLite3::DBSQLite3(const std::string &mapdir)
{
	int result;
	std::string db_name = mapdir + "map.sqlite";

	SQLOK(open_v2(db_name.c_str(), &db, SQLITE_OPEN_READONLY |
			SQLITE_OPEN_PRIVATECACHE, 0))

	SQLOK(prepare_v2(db,
			"SELECT pos, data FROM blocks WHERE pos BETWEEN ? AND ?",
		-1, &stmt_get_blocks_z, NULL))

	SQLOK(prepare_v2(db,
			"SELECT pos FROM blocks",
		-1, &stmt_get_block_pos, NULL))
}


DBSQLite3::~DBSQLite3()
{
	sqlite3_finalize(stmt_get_blocks_z);
	sqlite3_finalize(stmt_get_block_pos);

	if (sqlite3_close(db) != SQLITE_OK) {
		std::cerr << "Error closing SQlite DB\n";
	};
}

std::vector<BlockPos> DBSQLite3::getBlockPos()
{
	int result;
	std::vector<BlockPos> positions;
	while ((result = sqlite3_step(stmt_get_block_pos)) != SQLITE_DONE) {
		if (result == SQLITE_ROW) {
			int64_t posHash = sqlite3_column_int64(stmt_get_block_pos, 0);
			positions.push_back(decodeBlockPos(posHash));
		} else if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else {
			throw std::runtime_error(sqlite3_errmsg(db));
		}
	}
	SQLOK(reset(stmt_get_block_pos));
	return positions;
}


void DBSQLite3::getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos)
{
	int result;

	// Magic numbers!
	int64_t minPos = encodeBlockPos(BlockPos(0, -2048, zPos));
	int64_t maxPos = encodeBlockPos(BlockPos(0, 2048, zPos)) - 1;

	SQLOK(bind_int64(stmt_get_blocks_z, 1, minPos));
	SQLOK(bind_int64(stmt_get_blocks_z, 2, maxPos));

	while ((result = sqlite3_step(stmt_get_blocks_z)) != SQLITE_DONE) {
		if (result == SQLITE_ROW) {
			int64_t posHash = sqlite3_column_int64(stmt_get_blocks_z, 0);
			const unsigned char *data = reinterpret_cast<const unsigned char *>(
					sqlite3_column_blob(stmt_get_blocks_z, 1));
			size_t size = sqlite3_column_bytes(stmt_get_blocks_z, 1);
			Block b(decodeBlockPos(posHash), ustring(data, size));
			blocks[b.first.x].push_back(b);
		} else if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else {
			throw std::runtime_error(sqlite3_errmsg(db));
		}
	}
	SQLOK(reset(stmt_get_blocks_z));
}

#undef SQLRES
#undef SQLOK

