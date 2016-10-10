#ifndef DB_HEADER
#define DB_HEADER

#include <stdint.h>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <utility>
#include "types.h"


class BlockPos {
public:
	int16_t x;
	int16_t y;
	int16_t z;

	BlockPos() : x(0), y(0), z(0) {}
	BlockPos(int16_t x, int16_t y, int16_t z) : x(x), y(y), z(z) {}
	bool operator < (const BlockPos &p) const
	{
		if (z > p.z) {
			return true;
		}
		if (z < p.z) {
			return false;
		}
		if (y > p.y) {
			return true;
		}
		if (y < p.y) {
			return false;
		}
		if (x > p.x) {
			return true;
		}
		if (x < p.x) {
			return false;
		}
		return false;
	}
};


typedef std::pair<BlockPos, ustring> Block;
typedef std::list<Block> BlockList;


class DB {
protected:
	inline int64_t  encodeBlockPos(const BlockPos pos) const;
	inline BlockPos decodeBlockPos(int64_t hash) const;

public:
	virtual std::vector<BlockPos> getBlockPos() = 0;
	virtual void getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos) = 0;
	virtual ~DB() {};
};



/****************
 * Black magic! *
 ****************
 * The position hashing is seriously messed up,
 * and is a lot more complicated than it looks.
 */

static inline int16_t unsigned_to_signed(uint16_t i, uint16_t max_positive)
{
	if (i < max_positive) {
		return i;
	} else {
		return i - (max_positive * 2);
	}
}


// Modulo of a negative number does not work consistently in C
static inline int64_t pythonmodulo(int64_t i, int64_t mod)
{
	if (i >= 0) {
		return i % mod;
	}
	return mod - ((-i) % mod);
}


inline int64_t DB::encodeBlockPos(const BlockPos pos) const
{
	return (uint64_t) pos.z * 0x1000000 +
		(uint64_t) pos.y * 0x1000 +
		(uint64_t) pos.x;
}


inline BlockPos DB::decodeBlockPos(int64_t hash) const
{
	BlockPos pos;
	pos.x = unsigned_to_signed(pythonmodulo(hash, 4096), 2048);
	hash = (hash - pos.x) / 4096;
	pos.y = unsigned_to_signed(pythonmodulo(hash, 4096), 2048);
	hash = (hash - pos.y) / 4096;
	pos.z = unsigned_to_signed(pythonmodulo(hash, 4096), 2048);
	return pos;
}

/*******************
 * End black magic *
 *******************/

#endif // DB_HEADER
