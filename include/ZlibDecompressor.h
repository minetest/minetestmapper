#pragma once

#include <cstdlib>
#include <string>
#include "types.h"


class ZlibDecompressor
{
public:
	class DecompressError {
	};

	ZlibDecompressor(const unsigned char *data, std::size_t size);
	~ZlibDecompressor();
	void setSeekPos(std::size_t seekPos);
	std::size_t seekPos() const;
	ustring decompress();

private:
	const unsigned char *m_data;
	std::size_t m_seekPos;
	std::size_t m_size;
};
