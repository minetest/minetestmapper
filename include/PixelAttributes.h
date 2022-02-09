#pragma once

#include <climits>
#include <cstdint>

#define BLOCK_SIZE 16

struct PixelAttribute {
	PixelAttribute() : height(INT16_MIN), thickness(0) {};

	int16_t height;
	uint8_t thickness;

	inline bool valid_height() const {
		return height != INT16_MIN;
	}
};

class PixelAttributes
{
public:
	PixelAttributes();
	virtual ~PixelAttributes();

	void setWidth(int width);
	void scroll();

	inline PixelAttribute &attribute(int z, int x) {
		return m_pixelAttributes[z + 1][x + 1];
	};

private:
	void freeAttributes();

private:
	enum Line {
		FirstLine = 0,
		LastLine = BLOCK_SIZE,
		EmptyLine = BLOCK_SIZE + 1,
		LineCount = BLOCK_SIZE + 2
	};
	PixelAttribute *m_pixelAttributes[BLOCK_SIZE + 2]; // 1px gradient + empty
	int m_width;
};
