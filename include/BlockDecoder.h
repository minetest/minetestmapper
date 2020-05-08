#pragma once

#include <unordered_map>

#include "types.h"

class BlockDecoder {
public:
	BlockDecoder();

	void reset();
	void decode(const ustring &data);
	bool isEmpty() const;
	// returns "" for air, ignore and invalid nodes
	const std::string &getNode(u8 x, u8 y, u8 z) const;

private:
	typedef std::unordered_map<uint16_t, std::string> NameMap;
	NameMap m_nameMap;
	int m_blockAirId;
	int m_blockIgnoreId;

	u8 m_version, m_contentWidth;
	ustring m_mapData;
};
