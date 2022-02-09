#include <zlib.h>
#include <stdint.h>
#include "ZlibDecompressor.h"

ZlibDecompressor::ZlibDecompressor(const u8 *data, size_t size):
	m_data(data),
	m_seekPos(0),
	m_size(size)
{
}

ZlibDecompressor::~ZlibDecompressor()
{
}

void ZlibDecompressor::setSeekPos(size_t seekPos)
{
	m_seekPos = seekPos;
}

size_t ZlibDecompressor::seekPos() const
{
	return m_seekPos;
}

ustring ZlibDecompressor::decompress()
{
	const unsigned char *data = m_data + m_seekPos;
	const size_t size = m_size - m_seekPos;

	ustring buffer;
	constexpr size_t BUFSIZE = 128 * 1024;
	unsigned char temp_buffer[BUFSIZE];

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = Z_NULL;
	strm.avail_in = size;

	if (inflateInit(&strm) != Z_OK)
		throw DecompressError();

	strm.next_in = const_cast<unsigned char *>(data);
	int ret = 0;
	do {
		strm.avail_out = BUFSIZE;
		strm.next_out = temp_buffer;
		ret = inflate(&strm, Z_NO_FLUSH);
		buffer.append(temp_buffer, BUFSIZE - strm.avail_out);
	} while (ret == Z_OK);
	if (ret != Z_STREAM_END)
		throw DecompressError();

	m_seekPos += strm.next_in - data;
	(void) inflateEnd(&strm);

	return buffer;
}

