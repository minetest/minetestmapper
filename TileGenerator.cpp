#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <type_traits>
#include <limits>

#include "TileGenerator.h"
#include "config.h"
#include "PlayerAttributes.h"
#include "POIAttributes.h"
#include "BlockDecoder.h"
#include "Image.h"
#include "util.h"

#include "db-sqlite3.h"
#if USE_POSTGRESQL
#include "db-postgresql.h"
#endif
#if USE_LEVELDB
#include "db-leveldb.h"
#endif
#if USE_REDIS
#include "db-redis.h"
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// saturating multiplication
template<typename T, class = typename std::enable_if<std::is_unsigned<T>::value>::type>
inline T sat_mul(T a, T b)
{
#if __has_builtin(__builtin_mul_overflow)
	T res;
	if (__builtin_mul_overflow(a, b, &res))
		return std::numeric_limits<T>::max();
	return res;
#else
	// WARNING: the fallback implementation is incorrect since we compute ceil(log(x)) not log(x)
	// but that's good enough for our usecase...
	const int bits = sizeof(T) * 8;
	int hb_a = 0, hb_b = 0;
	for (int i = bits - 1; i >= 0; i--) {
		if (a & (static_cast<T>(1) << i)) {
			hb_a = i; break;
		}
	}
	for (int i = bits - 1; i >= 0; i--) {
		if (b & (static_cast<T>(1) << i)) {
			hb_b = i; break;
		}
	}
	// log2(a) + log2(b) >= log2(MAX) <=> calculation will overflow
	if (hb_a + hb_b >= bits)
		return std::numeric_limits<T>::max();
	return a * b;
#endif
}

template<typename T>
inline T sat_mul(T a, T b, T c)
{
	return sat_mul(sat_mul(a, b), c);
}

// rounds n (away from 0) to a multiple of f while preserving the sign of n
static int round_multiple_nosign(int n, int f)
{
	int abs_n, sign;
	abs_n = (n >= 0) ? n : -n;
	sign = (n >= 0) ? 1 : -1;
	if (abs_n % f == 0)
		return n; // n == abs_n * sign
	else
		return sign * (abs_n + f - (abs_n % f));
}

static inline unsigned int colorSafeBounds(int channel)
{
	return mymin(mymax(channel, 0), 255);
}

static Color parseColor(const std::string &color)
{
	if (color.length() != 7)
		throw std::runtime_error("Color needs to be 7 characters long");
	if (color[0] != '#')
		throw std::runtime_error("Color needs to begin with #");
	unsigned long col = strtoul(color.c_str() + 1, NULL, 16);
	u8 b, g, r;
	b = col & 0xff;
	g = (col >> 8) & 0xff;
	r = (col >> 16) & 0xff;
	return Color(r, g, b);
}

static Color mixColors(Color a, Color b)
{
	Color result;
	double a1 = a.a / 255.0;
	double a2 = b.a / 255.0;

	result.r = (int) (a1 * a.r + a2 * (1 - a1) * b.r);
	result.g = (int) (a1 * a.g + a2 * (1 - a1) * b.g);
	result.b = (int) (a1 * a.b + a2 * (1 - a1) * b.b);
	result.a = (int) (255 * (a1 + a2 * (1 - a1)));

	return result;
}

TileGenerator::TileGenerator():
	m_bgColor(255, 255, 255),
	m_scaleColor(0, 0, 0),
	m_originColor(255, 0, 0),
	m_playerColor(255, 0, 0),
	m_poiColor(0, 128, 255),
	m_drawOrigin(false),
	m_drawPlayers(false),
	m_drawScale(false),
	m_drawAlpha(false),
	m_shading(true),
	m_dontWriteEmpty(false),
	m_backend(""),
	m_xBorder(0),
	m_yBorder(0),
	m_db(NULL),
	m_image(NULL),
	m_xMin(INT_MAX),
	m_xMax(INT_MIN),
	m_zMin(INT_MAX),
	m_zMax(INT_MIN),
	m_yMin(INT16_MIN),
	m_yMax(INT16_MAX),
	m_geomX(-2048),
	m_geomY(-2048),
	m_geomX2(2048),
	m_geomY2(2048),
	m_exhaustiveSearch(EXH_AUTO),
	m_renderedAny(false),
	m_zoom(1),
	m_scales(SCALE_LEFT | SCALE_TOP),
	m_progressMax(0),
	m_progressLast(-1)
{
}

TileGenerator::~TileGenerator()
{
	closeDatabase();
}

void TileGenerator::setBgColor(const std::string &bgColor)
{
	m_bgColor = parseColor(bgColor);
}

void TileGenerator::setScaleColor(const std::string &scaleColor)
{
	m_scaleColor = parseColor(scaleColor);
}

void TileGenerator::setOriginColor(const std::string &originColor)
{
	m_originColor = parseColor(originColor);
}

void TileGenerator::setPlayerColor(const std::string &playerColor)
{
	m_playerColor = parseColor(playerColor);
}

void TileGenerator::setZoom(int zoom)
{
	if (zoom < 1)
		throw std::runtime_error("Zoom level needs to be a number: 1 or higher");
	m_zoom = zoom;
}

void TileGenerator::setScales(uint flags)
{
	m_scales = flags;
}

void TileGenerator::setDrawOrigin(bool drawOrigin)
{
	m_drawOrigin = drawOrigin;
}

void TileGenerator::setDrawPlayers(bool drawPlayers)
{
	m_drawPlayers = drawPlayers;
}

void TileGenerator::setDrawPOIs(bool drawPOIs)
{
   m_drawPOIs = drawPOIs;
}

void TileGenerator::setDrawScale(bool drawScale)
{
	m_drawScale = drawScale;
}

void TileGenerator::setDrawAlpha(bool drawAlpha)
{
	m_drawAlpha = drawAlpha;
}

void TileGenerator::setShading(bool shading)
{
	m_shading = shading;
}

void TileGenerator::setBackend(std::string backend)
{
	m_backend = backend;
}

void TileGenerator::setGeometry(int x, int y, int w, int h)
{
	assert(w > 0 && h > 0);
	m_geomX  = round_multiple_nosign(x, 16) / 16;
	m_geomY  = round_multiple_nosign(y, 16) / 16;
	m_geomX2 = round_multiple_nosign(x + w, 16) / 16;
	m_geomY2 = round_multiple_nosign(y + h, 16) / 16;
}

void TileGenerator::setMinY(int y)
{
	m_yMin = y;
	if (m_yMin > m_yMax)
		std::swap(m_yMin, m_yMax);
}

void TileGenerator::setMaxY(int y)
{
	m_yMax = y;
	if (m_yMin > m_yMax)
		std::swap(m_yMin, m_yMax);
}

void TileGenerator::setExhaustiveSearch(int mode)
{
	m_exhaustiveSearch = mode;
}

void TileGenerator::setDontWriteEmpty(bool f)
{
	m_dontWriteEmpty = f;
}

void TileGenerator::parseColorsFile(const std::string &fileName)
{
	std::ifstream in(fileName);
	if (!in.good())
		throw std::runtime_error("Specified colors file could not be found");
	parseColorsStream(in);
}

void TileGenerator::printGeometry(const std::string &input_path)
{
	setExhaustiveSearch(EXH_NEVER);
	openDb(input_path);
	loadBlocks();

	std::cout << "Map extent: "
		<< m_xMin*16 << ":" << m_zMin*16
		<< "+" << (m_xMax - m_xMin+1)*16
		<< "+" << (m_zMax - m_zMin+1)*16
		<< std::endl;

	closeDatabase();
}

void TileGenerator::dumpBlock(const std::string &input_path, BlockPos pos)
{
	openDb(input_path);

	BlockList list;
	std::vector<BlockPos> positions;
	positions.emplace_back(pos);
	m_db->getBlocksByPos(list, positions);
	if (!list.empty()) {
		const ustring &data = list.begin()->second;
		for (u8 c : data)
			printf("%02x", static_cast<int>(c));
		printf("\n");
	}

	closeDatabase();
}

void TileGenerator::generate(const std::string &input_path, const std::string &output)
{
	if (m_dontWriteEmpty) // FIXME: possible too, just needs to be done differently
		setExhaustiveSearch(EXH_NEVER);
	openDb(input_path);
	loadBlocks();

	if (m_dontWriteEmpty && m_positions.empty())
	{
		closeDatabase();
		return;
	}

	createImage();
	renderMap();
	closeDatabase();
	if (m_drawScale) {
		renderScale();
	}
	if (m_drawOrigin) {
		renderOrigin();
	}
	if (m_drawPlayers) {
		renderPlayers(input_path);
	}
	if (m_drawPOIs) {
	   renderPOIs(input_path);
	}
	writeImage(output);
	printUnknown();
}

void TileGenerator::parseColorsStream(std::istream &in)
{
	char line[512];
	while (in.good()) {
		in.getline(line, sizeof(line));

		for (char *p = line; *p; p++) {
			if (*p != '#')
				continue;
			*p = '\0'; // Cut off at the first #
			break;
		}
		if(!line[0])
			continue;

		char name[200 + 1] = {0};
		unsigned int r, g, b, a = 255, t = 0;
		int items = sscanf(line, "%200s %u %u %u %u %u", name, &r, &g, &b, &a, &t);
		if (items < 4) {
			std::cerr << "Failed to parse color entry '" << line << "'" << std::endl;
			continue;
		}

		m_colorMap[name] = ColorEntry(r, g, b, a, t);
	}
}

std::set<std::string> TileGenerator::getSupportedBackends()
{
	std::set<std::string> r;
	r.insert("sqlite3");
#if USE_POSTGRESQL
	r.insert("postgresql");
#endif
#if USE_LEVELDB
	r.insert("leveldb");
#endif
#if USE_REDIS
	r.insert("redis");
#endif
	return r;
}

void TileGenerator::openDb(const std::string &input_path)
{
	std::string input = input_path;
	if (input.back() != PATH_SEPARATOR)
		input += PATH_SEPARATOR;

	std::string backend = m_backend;
	if (backend.empty()) {
		std::ifstream ifs(input + "world.mt");
		if(!ifs.good())
			throw std::runtime_error("Failed to open world.mt");
		backend = read_setting_default("backend", ifs, "sqlite3");
		ifs.close();
	}

	if (backend == "sqlite3")
		m_db = new DBSQLite3(input);
#if USE_POSTGRESQL
	else if (backend == "postgresql")
		m_db = new DBPostgreSQL(input);
#endif
#if USE_LEVELDB
	else if (backend == "leveldb")
		m_db = new DBLevelDB(input);
#endif
#if USE_REDIS
	else if (backend == "redis")
		m_db = new DBRedis(input);
#endif
	else
		throw std::runtime_error(std::string("Unknown map backend: ") + backend);

	// Determine how we're going to traverse the database (heuristic)
	if (m_exhaustiveSearch == EXH_AUTO) {
		size_t y_range = (m_yMax / 16 + 1) - (m_yMin / 16);
		size_t blocks = sat_mul<size_t>(m_geomX2 - m_geomX, y_range, m_geomY2 - m_geomY);
#ifndef NDEBUG
		std::cerr << "Heuristic parameters:"
			<< " preferRangeQueries()=" << m_db->preferRangeQueries()
			<< " y_range=" << y_range << " blocks=" << blocks << std::endl;
#endif
		if (m_db->preferRangeQueries())
			m_exhaustiveSearch = EXH_NEVER;
		else if (blocks < 200000)
			m_exhaustiveSearch = EXH_FULL;
		else if (y_range < 42)
			m_exhaustiveSearch = EXH_Y;
		else
			m_exhaustiveSearch = EXH_NEVER;
	} else if (m_exhaustiveSearch == EXH_FULL || m_exhaustiveSearch == EXH_Y) {
		if (m_db->preferRangeQueries()) {
			std::cerr << "Note: The current database backend supports efficient "
				"range queries, forcing exhaustive search should always result "
				" in worse performance." << std::endl;
		}
	}
	assert(m_exhaustiveSearch != EXH_AUTO);
}

void TileGenerator::closeDatabase()
{
	delete m_db;
	m_db = NULL;
}

static inline int16_t mod16(int16_t y)
{
	if (y < 0)
		return (y - 15) / 16;
	return y / 16;
}

void TileGenerator::loadBlocks()
{
	const int16_t yMax = mod16(m_yMax) + 1;
	const int16_t yMin = mod16(m_yMin);

	if (m_exhaustiveSearch == EXH_NEVER || m_exhaustiveSearch == EXH_Y) {
		std::vector<BlockPos> vec = m_db->getBlockPos(
			BlockPos(m_geomX, yMin, m_geomY),
			BlockPos(m_geomX2, yMax, m_geomY2)
		);

		for (auto pos : vec) {
			assert(pos.x >= m_geomX && pos.x < m_geomX2);
			assert(pos.y >= yMin && pos.y < yMax);
			assert(pos.z >= m_geomY && pos.z < m_geomY2);

			// Adjust minimum and maximum positions to the nearest block
			if (pos.x < m_xMin)
				m_xMin = pos.x;
			if (pos.x > m_xMax)
				m_xMax = pos.x;

			if (pos.z < m_zMin)
				m_zMin = pos.z;
			if (pos.z > m_zMax)
				m_zMax = pos.z;

			m_positions[pos.z].emplace(pos.x);
		}

		size_t count = 0;
		for (const auto &it : m_positions)
			count += it.second.size();
		m_progressMax = count;
#ifndef NDEBUG
		std::cerr << "Loaded " << count
			<< " positions (across Z: " << m_positions.size() << ") for rendering" << std::endl;
#endif
	}
}

void TileGenerator::createImage()
{
	const int scale_d = 40; // pixels reserved for a scale
	if(!m_drawScale)
		m_scales = 0;

	// If a geometry is explicitly set, set the bounding box to the requested geometry
	// instead of cropping to the content. This way we will always output a full tile
	// of the correct size.
	if (m_geomX > -2048 && m_geomX2 < 2048)
	{
		m_xMin = m_geomX;
		m_xMax = m_geomX2-1;
	}

	if (m_geomY > -2048 && m_geomY2 < 2048)
	{
		m_zMin = m_geomY;
		m_zMax = m_geomY2-1;
	}

	m_mapWidth = (m_xMax - m_xMin + 1) * 16;
	m_mapHeight = (m_zMax - m_zMin + 1) * 16;

	m_xBorder = (m_scales & SCALE_LEFT) ? scale_d : 0;
	m_yBorder = (m_scales & SCALE_TOP) ? scale_d : 0;
	m_blockPixelAttributes.setWidth(m_mapWidth);

	int image_width, image_height;
	image_width = (m_mapWidth * m_zoom) + m_xBorder;
	image_width += (m_scales & SCALE_RIGHT) ? scale_d : 0;
	image_height = (m_mapHeight * m_zoom) + m_yBorder;
	image_height += (m_scales & SCALE_BOTTOM) ? scale_d : 0;

	if(image_width > 4096 || image_height > 4096) {
		std::cerr << "Warning: The width or height of the image to be created exceeds 4096 pixels!"
			<< " (Dimensions: " << image_width << "x" << image_height << ")"
			<< std::endl;
	}
	m_image = new Image(image_width, image_height);
	m_image->drawFilledRect(0, 0, image_width, image_height, m_bgColor); // Background
}

void TileGenerator::renderMap()
{
	BlockDecoder blk;
	const int16_t yMax = mod16(m_yMax) + 1;
	const int16_t yMin = mod16(m_yMin);
	size_t count = 0;

	auto renderSingle = [&] (int16_t xPos, int16_t zPos, BlockList &blockStack) {
		m_readPixels.reset();
		m_readInfo.reset();
		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				m_color[i][j] = m_bgColor; // This will be drawn by renderMapBlockBottom() for y-rows with only 'air', 'ignore' or unknown nodes if --drawalpha is used
				m_color[i][j].a = 0; // ..but set alpha to 0 to tell renderMapBlock() not to use this color to mix a shade
				m_thickness[i][j] = 0;
			}
		}

		for (const auto &it : blockStack) {
			const BlockPos pos = it.first;
			assert(pos.x == xPos && pos.z == zPos);
			assert(pos.y >= yMin && pos.y < yMax);

			blk.reset();
			blk.decode(it.second);
			if (blk.isEmpty())
				continue;
			renderMapBlock(blk, pos);

			// Exit out if all pixels for this MapBlock are covered
			if (m_readPixels.full())
				break;
		}
		if (!m_readPixels.full())
			renderMapBlockBottom(blockStack.begin()->first);
		m_renderedAny |= m_readInfo.any();
	};
	auto postRenderRow = [&] (int16_t zPos) {
		if (m_shading)
			renderShading(zPos);
	};

	if (m_exhaustiveSearch == EXH_NEVER) {
		for (auto it = m_positions.rbegin(); it != m_positions.rend(); ++it) {
			int16_t zPos = it->first;
			for (auto it2 = it->second.rbegin(); it2 != it->second.rend(); ++it2) {
				int16_t xPos = *it2;

				BlockList blockStack;
				m_db->getBlocksOnXZ(blockStack, xPos, zPos, yMin, yMax);
				blockStack.sort();

				renderSingle(xPos, zPos, blockStack);
				reportProgress(count++);
			}
			postRenderRow(zPos);
		}
	} else if (m_exhaustiveSearch == EXH_Y) {
#ifndef NDEBUG
		std::cerr << "Exhaustively searching height of "
			<< (yMax - yMin) << " blocks" << std::endl;
#endif
		std::vector<BlockPos> positions;
		positions.reserve(yMax - yMin);
		for (auto it = m_positions.rbegin(); it != m_positions.rend(); ++it) {
			int16_t zPos = it->first;
			for (auto it2 = it->second.rbegin(); it2 != it->second.rend(); ++it2) {
				int16_t xPos = *it2;

				positions.clear();
				for (int16_t yPos = yMin; yPos < yMax; yPos++)
					positions.emplace_back(xPos, yPos, zPos);

				BlockList blockStack;
				m_db->getBlocksByPos(blockStack, positions);
				blockStack.sort();

				renderSingle(xPos, zPos, blockStack);
				reportProgress(count++);
			}
			postRenderRow(zPos);
		}
	} else if (m_exhaustiveSearch == EXH_FULL) {
		const size_t span_y = yMax - yMin;
		m_progressMax = (m_geomX2 - m_geomX) * span_y * (m_geomY2 - m_geomY);
#ifndef NDEBUG
		std::cerr << "Exhaustively searching "
			<< (m_geomX2 - m_geomX) << "x" << span_y << "x"
			<< (m_geomY2 - m_geomY) << " blocks" << std::endl;
#endif

		std::vector<BlockPos> positions;
		positions.reserve(span_y);
		for (int16_t zPos = m_geomY2 - 1; zPos >= m_geomY; zPos--) {
			for (int16_t xPos = m_geomX2 - 1; xPos >= m_geomX; xPos--) {
				positions.clear();
				for (int16_t yPos = yMin; yPos < yMax; yPos++)
					positions.emplace_back(xPos, yPos, zPos);

				BlockList blockStack;
				m_db->getBlocksByPos(blockStack, positions);
				blockStack.sort();

				renderSingle(xPos, zPos, blockStack);
				reportProgress(count++);
			}
			postRenderRow(zPos);
		}
	}

	reportProgress(m_progressMax);
}

void TileGenerator::renderMapBlock(const BlockDecoder &blk, const BlockPos &pos)
{
	int xBegin = (pos.x - m_xMin) * 16;
	int zBegin = (m_zMax - pos.z) * 16;
	int minY = (pos.y * 16 > m_yMin) ? 0 : m_yMin - pos.y * 16;
	int maxY = (pos.y * 16 + 15 < m_yMax) ? 15 : m_yMax - pos.y * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + 15 - z;
		for (int x = 0; x < 16; ++x) {
			if (m_readPixels.get(x, z))
				continue;
			int imageX = xBegin + x;
			auto &attr = m_blockPixelAttributes.attribute(15 - z, xBegin + x);

			for (int y = maxY; y >= minY; --y) {
				const std::string &name = blk.getNode(x, y, z);
				if (name.empty())
					continue;
				ColorMap::const_iterator it = m_colorMap.find(name);
				if (it == m_colorMap.end()) {
					m_unknownNodes.insert(name);
					continue;
				}

				Color c = it->second.toColor();
				if (c.a == 0)
					continue; // node is fully invisible
				if (m_drawAlpha) {
					if (m_color[z][x].a != 0)
						c = mixColors(m_color[z][x], c);
					if (c.a < 255) {
						// remember color and near thickness value
						m_color[z][x] = c;
						m_thickness[z][x] = (m_thickness[z][x] + it->second.t) / 2;
						continue;
					}
					// color became opaque, draw it
					setZoomed(imageX, imageY, c);
					attr.thickness = m_thickness[z][x];
				} else {
					c.a = 255;
					setZoomed(imageX, imageY, c);
				}
				m_readPixels.set(x, z);

				// do this afterwards so we can record height values
				// inside transparent nodes (water) too
				if (!m_readInfo.get(x, z)) {
					attr.height = pos.y * 16 + y;
					m_readInfo.set(x, z);
				}
				break;
			}
		}
	}
}

void TileGenerator::renderMapBlockBottom(const BlockPos &pos)
{
	if (!m_drawAlpha)
		return; // "missing" pixels can only happen with --drawalpha

	int xBegin = (pos.x - m_xMin) * 16;
	int zBegin = (m_zMax - pos.z) * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + 15 - z;
		for (int x = 0; x < 16; ++x) {
			if (m_readPixels.get(x, z))
				continue;
			int imageX = xBegin + x;
			auto &attr = m_blockPixelAttributes.attribute(15 - z, xBegin + x);

			// set color since it wasn't done in renderMapBlock()
			setZoomed(imageX, imageY, m_color[z][x]);
			m_readPixels.set(x, z);
			attr.thickness = m_thickness[z][x];
		}
	}
}

void TileGenerator::renderShading(int zPos)
{
	auto &a = m_blockPixelAttributes;
	int zBegin = (m_zMax - zPos) * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + z;
		if (imageY >= m_mapHeight)
			continue;
		for (int x = 0; x < m_mapWidth; ++x) {
			if(
				!a.attribute(z, x).valid_height() ||
				!a.attribute(z, x - 1).valid_height() ||
				!a.attribute(z - 1, x).valid_height()
			)
				continue;

			// calculate shadow to apply
			int y = a.attribute(z, x).height;
			int y1 = a.attribute(z, x - 1).height;
			int y2 = a.attribute(z - 1, x).height;
			int d = ((y - y1) + (y - y2)) * 12;

			if (m_drawAlpha) { // less visible shadow with increasing "thickness"
				float t = a.attribute(z, x).thickness * 1.2f;
				t = mymin(t, 255.0f);
				d *= 1.0f - t / 255.0f;
			}

			d = mymin(d, 36);

			// apply shadow/light by just adding to it pixel values
			Color c = m_image->getPixel(getImageX(x), getImageY(imageY));
			c.r = colorSafeBounds(c.r + d);
			c.g = colorSafeBounds(c.g + d);
			c.b = colorSafeBounds(c.b + d);
			setZoomed(x, imageY, c);
		}
	}
	a.scroll();
}

void TileGenerator::renderScale()
{
	const int scale_d = 40; // see createImage()

	if (m_scales & SCALE_TOP) {
		m_image->drawText(24, 0, "X", m_scaleColor);
		for (int i = (m_xMin / 4) * 4; i <= m_xMax; i += 4) {
			std::ostringstream buf;
			buf << i * 16;

			int xPos = getImageX(i * 16, true);
			if (xPos >= 0) {
				m_image->drawText(xPos + 2, 0, buf.str(), m_scaleColor);
				m_image->drawLine(xPos, 0, xPos, m_yBorder - 1, m_scaleColor);
			}
		}
	}

	if (m_scales & SCALE_LEFT) {
		m_image->drawText(2, 24, "Z", m_scaleColor);
		for (int i = (m_zMax / 4) * 4; i >= m_zMin; i -= 4) {
			std::ostringstream buf;
			buf << i * 16;

			int yPos = getImageY(i * 16 + 1, true);
			if (yPos >= 0) {
				m_image->drawText(2, yPos, buf.str(), m_scaleColor);
				m_image->drawLine(0, yPos, m_xBorder - 1, yPos, m_scaleColor);
			}
		}
	}

	if (m_scales & SCALE_BOTTOM) {
		int xPos = m_xBorder + m_mapWidth*m_zoom - 24 - 8,
			yPos = m_yBorder + m_mapHeight*m_zoom + scale_d - 12;
		m_image->drawText(xPos, yPos, "X", m_scaleColor);
		for (int i = (m_xMin / 4) * 4; i <= m_xMax; i += 4) {
			std::ostringstream buf;
			buf << i * 16;

			xPos = getImageX(i * 16, true);
			yPos = m_yBorder + m_mapHeight*m_zoom;
			if (xPos >= 0) {
				m_image->drawText(xPos + 2, yPos, buf.str(), m_scaleColor);
				m_image->drawLine(xPos, yPos, xPos, yPos + 39, m_scaleColor);
			}
		}
	}

	if (m_scales & SCALE_RIGHT) {
		int xPos = m_xBorder + m_mapWidth*m_zoom + scale_d - 2 - 8,
			yPos = m_yBorder + m_mapHeight*m_zoom - 24 - 12;
		m_image->drawText(xPos, yPos, "Z", m_scaleColor);
		for (int i = (m_zMax / 4) * 4; i >= m_zMin; i -= 4) {
			std::ostringstream buf;
			buf << i * 16;

			xPos = m_xBorder + m_mapWidth*m_zoom;
			yPos = getImageY(i * 16 + 1, true);
			if (yPos >= 0) {
				m_image->drawText(xPos + 2, yPos, buf.str(), m_scaleColor);
				m_image->drawLine(xPos, yPos, xPos + 39, yPos, m_scaleColor);
			}
		}
	}
}

void TileGenerator::renderOrigin()
{
	if (m_xMin > 0 || m_xMax < 0 ||
		m_zMin > 0 || m_zMax < 0)
		return;
	m_image->drawCircle(getImageX(0, true), getImageY(0, true), 12, m_originColor);
}

void TileGenerator::renderPlayers(const std::string &input_path)
{
	std::string input = input_path;
	if (input.back() != PATH_SEPARATOR)
		input += PATH_SEPARATOR;

	PlayerAttributes players(input);
	for (auto &player : players) {
		if (player.x < m_xMin * 16 || player.x > m_xMax * 16 ||
			player.z < m_zMin * 16 || player.z > m_zMax * 16)
			continue;
		if (player.y < m_yMin || player.y > m_yMax)
			continue;
		int imageX = getImageX(player.x, true),
			imageY = getImageY(player.z, true);

		m_image->drawFilledRect(imageX - 1, imageY, 3, 1, m_playerColor);
		m_image->drawFilledRect(imageX, imageY - 1, 1, 3, m_playerColor);
		m_image->drawText(imageX + 2, imageY, player.name, m_playerColor);
	}
}

void TileGenerator::renderPOIs(const std::string &input_path)
{
	std::string input = input_path;
	if (input.back() != PATH_SEPARATOR)
		input += PATH_SEPARATOR;

	POIAttributes pois(input);
	for (auto &poi : pois) {
	   if (poi.x < m_xMin * 16 || poi.x > m_xMax * 16 ||
		 poi.z < m_zMin * 16 || poi.z > m_zMax * 16)
	      continue;
	   if (poi.y < m_yMin || poi.y > m_yMax)
	      continue;
	   int imageX = getImageX(poi.x, true),
	       imageY = getImageY(poi.z, true);

	   m_image->drawCircle(imageX, imageY, 9, m_poiColor);
	   m_image->drawText(imageX + 7, imageY, poi.name, m_poiColor);
	}
}

void TileGenerator::writeImage(const std::string &output)
{
	m_image->save(output);
	delete m_image;
	m_image = nullptr;
}

void TileGenerator::printUnknown()
{
	if (m_unknownNodes.empty())
		return;
	std::cerr << "Unknown nodes:" << std::endl;
	for (const auto &node : m_unknownNodes)
		std::cerr << "\t" << node << std::endl;
	if (!m_renderedAny) {
		std::cerr << "The map was read successfully and not empty, but none of the "
			"encountered nodes had a color associated.\nCheck that you're using "
			"the right colors.txt. It should match the game you have installed." << std::endl;
	}
}

void TileGenerator::reportProgress(size_t count)
{
	if (!m_progressMax)
		return;
	int percent = count / static_cast<float>(m_progressMax) * 100;
	if (percent == m_progressLast)
		return;
	m_progressLast = percent;

	// Print a nice-looking ASCII progress bar
	char bar[51] = {0};
	memset(bar, ' ', 50);
	int i = 0, j = percent;
	for (; j >= 2; j -= 2)
		bar[i++] = '=';
	if (j)
		bar[i++] = '-';
	std::cout << "[" << bar << "] " << percent << "% " << (percent == 100 ? "\n" : "\r");
	std::cout.flush();
}

inline int TileGenerator::getImageX(int val, bool absolute) const
{
	if (absolute)
		val = (val - m_xMin * 16);
	return (m_zoom*val) + m_xBorder;
}

inline int TileGenerator::getImageY(int val, bool absolute) const
{
	if (absolute)
		val = m_mapHeight - (val - m_zMin * 16); // Z axis is flipped on image
	return (m_zoom*val) + m_yBorder;
}

inline void TileGenerator::setZoomed(int x, int y, Color color)
{
	m_image->drawFilledRect(getImageX(x), getImageY(y), m_zoom, m_zoom, color);
}
