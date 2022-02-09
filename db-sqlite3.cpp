#include <stdexcept>
#include <unistd.h> // for usleep
#include <iostream>
#include <algorithm>
#include <time.h>
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
			"SELECT data FROM blocks WHERE pos = ?",
		-1, &stmt_get_block_exact, NULL))

	SQLOK(prepare_v2(db,
			"SELECT pos FROM blocks",
		-1, &stmt_get_block_pos, NULL))

	SQLOK(prepare_v2(db,
			"SELECT pos FROM blocks WHERE pos BETWEEN ? AND ?",
		-1, &stmt_get_block_pos_z, NULL))
}


DBSQLite3::~DBSQLite3()
{
	sqlite3_finalize(stmt_get_blocks_z);
	sqlite3_finalize(stmt_get_block_pos);
	sqlite3_finalize(stmt_get_block_pos_z);
	sqlite3_finalize(stmt_get_block_exact);

	if (sqlite3_close(db) != SQLITE_OK) {
		std::cerr << "Error closing SQLite database." << std::endl;
	};
}


inline void DBSQLite3::getPosRange(int64_t &min, int64_t &max, int16_t zPos,
		int16_t zPos2) const
{
	/* The range of block positions is [-2048, 2047], which turns into [0, 4095]
	 * when casted to unsigned. This didn't actually help me understand the
	 * numbers below, but I wanted to write it down.
	 */

	// Magic numbers!
	min = encodeBlockPos(BlockPos(0, -2048, zPos));
	max = encodeBlockPos(BlockPos(0, 2048, zPos2)) - 1;
}


std::vector<BlockPos> DBSQLite3::getBlockPos(BlockPos min, BlockPos max)
{
	int result;
	sqlite3_stmt *stmt;

	if(min.z <= -2048 && max.z >= 2048) {
		stmt = stmt_get_block_pos;
	} else {
		stmt = stmt_get_block_pos_z;
		int64_t minPos, maxPos;
		if (min.z < -2048)
			min.z = -2048;
		if (max.z > 2048)
			max.z = 2048;
		getPosRange(minPos, maxPos, min.z, max.z - 1);
		SQLOK(bind_int64(stmt, 1, minPos))
		SQLOK(bind_int64(stmt, 2, maxPos))
	}

	std::vector<BlockPos> positions;
	while ((result = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		}

		int64_t posHash = sqlite3_column_int64(stmt, 0);
		BlockPos pos = decodeBlockPos(posHash);
		if(pos.x >= min.x && pos.x < max.x && pos.y >= min.y && pos.y < max.y)
			positions.emplace_back(pos);
	}
	SQLOK(reset(stmt));
	return positions;
}


void DBSQLite3::loadBlockCache(int16_t zPos)
{
	int result;
	blockCache.clear();

	int64_t minPos, maxPos;
	getPosRange(minPos, maxPos, zPos, zPos);

	SQLOK(bind_int64(stmt_get_blocks_z, 1, minPos));
	SQLOK(bind_int64(stmt_get_blocks_z, 2, maxPos));

	while ((result = sqlite3_step(stmt_get_blocks_z)) != SQLITE_DONE) {
		if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		}

		int64_t posHash = sqlite3_column_int64(stmt_get_blocks_z, 0);
		BlockPos pos = decodeBlockPos(posHash);
		const unsigned char *data = reinterpret_cast<const unsigned char *>(
				sqlite3_column_blob(stmt_get_blocks_z, 1));
		size_t size = sqlite3_column_bytes(stmt_get_blocks_z, 1);
		blockCache[pos.x].emplace_back(pos, ustring(data, size));
	}
	SQLOK(reset(stmt_get_blocks_z))
}


void DBSQLite3::getBlocksOnXZ(BlockList &blocks, int16_t x, int16_t z,
		int16_t min_y, int16_t max_y)
{
	/* Cache the blocks on the given Z coordinate between calls, this only
	 * works due to order in which the TileGenerator asks for blocks. */
	if (z != blockCachedZ) {
		loadBlockCache(z);
		blockCachedZ = z;
	}

	auto it = blockCache.find(x);
	if (it == blockCache.end())
		return;

	if (it->second.empty()) {
		/* We have swapped this list before, this is not supposed to happen
		 * because it's bad for performance. But rather than silently breaking
		 * do the right thing and load the blocks again. */
#ifndef NDEBUG
		std::cerr << "Warning: suboptimal access pattern for sqlite3 backend" << std::endl;
#endif
		loadBlockCache(z);
	}
	// Swap lists to avoid copying contents
	blocks.clear();
	std::swap(blocks, it->second);

	for (auto it = blocks.begin(); it != blocks.end(); ) {
		if (it->first.y < min_y || it->first.y >= max_y)
			it = blocks.erase(it);
		else
			it++;
	}
}


void DBSQLite3::getBlocksByPos(BlockList &blocks,
			const std::vector<BlockPos> &positions)
{
	int result;

	for (auto pos : positions) {
		int64_t dbPos = encodeBlockPos(pos);
		SQLOK(bind_int64(stmt_get_block_exact, 1, dbPos));

		while ((result = sqlite3_step(stmt_get_block_exact)) == SQLITE_BUSY) {
			usleep(10000); // Wait some time and try again
		}
		if (result == SQLITE_DONE) {
			// no data
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		} else {
			const unsigned char *data = reinterpret_cast<const unsigned char *>(
					sqlite3_column_blob(stmt_get_block_exact, 0));
			size_t size = sqlite3_column_bytes(stmt_get_block_exact, 0);
			blocks.emplace_back(pos, ustring(data, size));
		}

		SQLOK(reset(stmt_get_block_exact))
	}
}
