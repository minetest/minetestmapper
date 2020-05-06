#ifndef BLOCKDECODER_H
#define BLOCKDECODER_H

#include <unordered_map>

#include "types.h"

class BlockDecoder {
public:
	BlockDecoder();

	void reset();
	void decode(const ustring &data);
	bool isEmpty() const;
	std::string getNode(u8 x, u8 y, u8 z) const; // returns "" for air, ignore and invalid nodes

private:
	typedef std::unordered_map<int, std::string> NameMap;
	NameMap m_nameMap;
	int m_blockAirId;
	int m_blockIgnoreId;

	u8 m_version, m_contentWidth;
	ustring m_mapData;
};

#endif // BLOCKDECODER_H
