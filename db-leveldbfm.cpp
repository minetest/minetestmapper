#include <sstream>
#include "db-leveldbfm.h"
#include "types.h"

static inline int64_t stoi64(const std::string &s)
{
	std::stringstream tmp(s);
	int64_t t;
	tmp >> t;
	return t;
}

DBLevelDBFM::DBLevelDBFM(const std::string &mapdir) : DBLevelDB(mapdir, 1)
{
	loadPosCache();
}

DBLevelDBFM::~DBLevelDBFM()
{
}

std::string getBlockAsString(const BlockPos &pos) {
	std::ostringstream os;
	os << "a" << pos.x << "," << pos.y << "," << pos.z;
	return os.str().c_str();
}

BlockPos DBLevelDBFM::getStringAsBlock(const std::string &i)  {
	std::istringstream is(i);
	BlockPos pos;
	char c;
	if (i[0] == 'a') {
		is >> c; // 'a'
		is >> pos.x;
		is >> c; // ','
		is >> pos.y;
		is >> c; // ','
		is >> pos.z;
	} else { // old format
		return decodeBlockPos(stoi64(i));
	}
	return pos;
}

void DBLevelDBFM::loadPosCache()
{
	leveldb::Iterator * it = db->NewIterator(leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		posCache.push_back(getStringAsBlock(it->key().ToString()));
	}
	delete it;
}

void DBLevelDBFM::getBlocksOnZ(std::map<int16_t, BlockList> &blocks, int16_t zPos)
{
	std::string datastr;
	leveldb::Status status;

	for (std::vector<BlockPos>::iterator it = posCache.begin(); it != posCache.end(); ++it) {
		if (it->z != zPos) {
			continue;
		}
		status = db->Get(leveldb::ReadOptions(), getBlockAsString(*it), &datastr);
		if (status.ok()) {
			Block b(*it, ustring((const unsigned char *) datastr.data(), datastr.size()));
			blocks[b.first.x].push_back(b);
		}
	}
}
