#pragma once

#include <cstdint>
#include <unordered_map>
#include "types.h"
#include <ZstdDecompressor.h>

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
	uint16_t m_blockAirId, m_blockIgnoreId;

	u8 m_version, m_contentWidth;
	ustring m_mapData;

	// one instance for performance
	ZstdDecompressor m_zstd_decompressor;
};
