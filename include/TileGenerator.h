#pragma once

#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <cstdint>
#include <string>

#include "PixelAttributes.h"
#include "Image.h"
#include "db.h"
#include "types.h"

class BlockDecoder;
class Image;

enum {
	SCALE_TOP = (1 << 0),
	SCALE_BOTTOM = (1 << 1),
	SCALE_LEFT = (1 << 2),
	SCALE_RIGHT = (1 << 3),
};

enum {
	EXH_NEVER, // Always use range queries
	EXH_Y,     // Exhaustively search Y space, range queries for X/Z
	EXH_FULL,  // Exhaustively search entire requested geometry
	EXH_AUTO,  // Automatically pick one of the previous modes
};

struct ColorEntry {
	ColorEntry() : r(0), g(0), b(0), a(0), t(0) {};
	ColorEntry(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t t) :
		r(r), g(g), b(b), a(a), t(t) {};
	inline Color toColor() const { return Color(r, g, b, a); }
	uint8_t r, g, b, a; // Red, Green, Blue, Alpha
	uint8_t t; // "thickness" value
};

struct BitmapThing { // 16x16 bitmap
	inline void reset() {
		for (int i = 0; i < 16; ++i)
			val[i] = 0;
	}
	inline bool any_neq(uint16_t v) const {
		for (int i = 0; i < 16; ++i) {
			if (val[i] != v)
				return true;
		}
		return false;
	}
	inline bool any() const { return any_neq(0); }
	inline bool full() const { return !any_neq(0xffff); }
	inline void set(unsigned int x, unsigned int z) {
		val[z] |= (1 << x);
	}
	inline bool get(unsigned int x, unsigned int z) const {
		return !!(val[z] & (1 << x));
	}

	uint16_t val[16];
};


class TileGenerator
{
private:
	typedef std::unordered_map<std::string, ColorEntry> ColorMap;

public:
	TileGenerator();
	~TileGenerator();
	void setBgColor(const std::string &bgColor);
	void setScaleColor(const std::string &scaleColor);
	void setOriginColor(const std::string &originColor);
	void setPlayerColor(const std::string &playerColor);
	void setDrawOrigin(bool drawOrigin);
	void setDrawPlayers(bool drawPlayers);
	void setDrawScale(bool drawScale);
	void setDrawAlpha(bool drawAlpha);
	void setShading(bool shading);
	void setGeometry(int x, int y, int w, int h);
	void setMinY(int y);
	void setMaxY(int y);
	void setExhaustiveSearch(int mode);
	void parseColorsFile(const std::string &fileName);
	void setBackend(std::string backend);
	void setZoom(int zoom);
	void setScales(uint flags);
	void setDontWriteEmpty(bool f);

	void generate(const std::string &input, const std::string &output);
	void printGeometry(const std::string &input);
	void dumpBlock(const std::string &input, BlockPos pos);

	static std::set<std::string> getSupportedBackends();

private:
	void parseColorsStream(std::istream &in);
	void openDb(const std::string &input);
	void closeDatabase();
	void loadBlocks();
	void createImage();
	void renderMap();
	void renderMapBlock(const BlockDecoder &blk, const BlockPos &pos);
	void renderMapBlockBottom(const BlockPos &pos);
	void renderShading(int zPos);
	void renderScale();
	void renderOrigin();
	void renderPlayers(const std::string &inputPath);
	void writeImage(const std::string &output);
	void printUnknown();
	void reportProgress(size_t count);
	int getImageX(int val, bool absolute=false) const;
	int getImageY(int val, bool absolute=false) const;
	void setZoomed(int x, int y, Color color);

private:
	Color m_bgColor;
	Color m_scaleColor;
	Color m_originColor;
	Color m_playerColor;
	bool m_drawOrigin;
	bool m_drawPlayers;
	bool m_drawScale;
	bool m_drawAlpha;
	bool m_shading;
	bool m_dontWriteEmpty;
	std::string m_backend;
	int m_xBorder, m_yBorder;

	DB *m_db;
	Image *m_image;
	PixelAttributes m_blockPixelAttributes;
	/* smallest/largest seen X or Z block coordinate */
	int m_xMin;
	int m_xMax;
	int m_zMin;
	int m_zMax;
	/* Y limits for rendered area (node units) */
	int m_yMin;
	int m_yMax;
	/* limits for rendered area (block units) */
	int16_t m_geomX;
	int16_t m_geomY; /* Y in terms of rendered image, Z in the world */
	int16_t m_geomX2;
	int16_t m_geomY2;

	int m_mapWidth;
	int m_mapHeight;
	int m_exhaustiveSearch;
	std::set<std::string> m_unknownNodes;
	bool m_renderedAny;
	std::map<int16_t, std::set<int16_t>> m_positions; /* indexed by Z, contains X coords */
	ColorMap m_colorMap;
	BitmapThing m_readPixels;
	BitmapThing m_readInfo;
	Color m_color[16][16];
	uint8_t m_thickness[16][16];

	int m_zoom;
	uint m_scales;

	size_t m_progressMax;
	int m_progressLast; // percentage
}; // class TileGenerator
