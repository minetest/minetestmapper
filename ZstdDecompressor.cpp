#include <zstd.h>
#include "ZstdDecompressor.h"

ZstdDecompressor::ZstdDecompressor():
	m_data(nullptr),
	m_seekPos(0),
	m_size(0)
{
	m_stream = ZSTD_createDStream();
}

ZstdDecompressor::~ZstdDecompressor()
{
	ZSTD_freeDStream(reinterpret_cast<ZSTD_DStream*>(m_stream));
}

void ZstdDecompressor::setData(const u8 *data, size_t size, size_t seekPos)
{
	m_data = data;
	m_seekPos = seekPos;
	m_size = size;
}

std::size_t ZstdDecompressor::seekPos() const
{
	return m_seekPos;
}

ustring ZstdDecompressor::decompress()
{
	ZSTD_DStream *stream = reinterpret_cast<ZSTD_DStream*>(m_stream);
	ZSTD_inBuffer inbuf = { m_data, m_size, m_seekPos };

	ustring buffer;
	constexpr size_t BUFSIZE = 32 * 1024;

	buffer.resize(BUFSIZE);
	ZSTD_outBuffer outbuf = { &buffer[0], buffer.size(), 0 };

	ZSTD_initDStream(stream);

	size_t ret;
	do {
		ret = ZSTD_decompressStream(stream, &outbuf, &inbuf);
		if (outbuf.size == outbuf.pos) {
			outbuf.size += BUFSIZE;
			buffer.resize(outbuf.size);
			outbuf.dst = &buffer[0];
		}
		if (ret && ZSTD_isError(ret))
			throw DecompressError();
	} while (ret != 0);

	m_seekPos = inbuf.pos;
	buffer.resize(outbuf.pos);

	return buffer;
}
