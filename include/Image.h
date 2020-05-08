#pragma once

#include "types.h"
#include <string>
#include <gd.h>

struct Color {
	Color() : r(0), g(0), b(0), a(0) {};
	Color(u8 r, u8 g, u8 b) : r(r), g(g), b(b), a(255) {};
	Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {};

	u8 r, g, b, a;
};

class Image {
public:
	Image(int width, int height);
	~Image();

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	void setPixel(int x, int y, const Color &c);
	Color getPixel(int x, int y);
	void drawLine(int x1, int y1, int x2, int y2, const Color &c);
	void drawText(int x, int y, const std::string &s, const Color &c);
	void drawFilledRect(int x, int y, int w, int h, const Color &c);
	void drawCircle(int x, int y, int diameter, const Color &c);
	void save(const std::string &filename);

private:
	int m_width, m_height;
	gdImagePtr m_image;
};
