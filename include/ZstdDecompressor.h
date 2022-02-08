#pragma once

#include <exception>
#include "types.h"

class ZstdDecompressor
{
public:
	class DecompressError : std::exception {};

	ZstdDecompressor();
	~ZstdDecompressor();
	void setData(const u8 *data, size_t size, size_t seekPos);
	size_t seekPos() const;
	ustring decompress();

private:
	void *m_stream; // ZSTD_DStream
	const u8 *m_data;
	size_t m_seekPos, m_size;
};
