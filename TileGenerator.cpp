#include <cstdio>
#include <cstdlib>
#include <climits>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>
#include "config.h"
#include "PlayerAttributes.h"
#include "TileGenerator.h"
#include "ZlibDecompressor.h"
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

using namespace std;

static inline uint16_t readU16(const unsigned char *data)
{
	return data[0] << 8 | data[1];
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

static int readBlockContent(const unsigned char *mapData, int version, int datapos)
{
	if (version >= 24) {
		size_t index = datapos << 1;
		return (mapData[index] << 8) | mapData[index + 1];
	} else if (version >= 20) {
		if (mapData[datapos] <= 0x80)
			return mapData[datapos];
		else
			return (int(mapData[datapos]) << 4) | (int(mapData[datapos + 0x2000]) >> 4);
	}
	std::ostringstream oss;
	oss << "Unsupported map version " << version;
	throw std::runtime_error(oss.str());
}

static inline int colorSafeBounds(int color)
{
	color = (color > 255) ? 255 : color;
	color = (color < 0) ? 0 : color;
	return color;
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
	m_drawOrigin(false),
	m_drawPlayers(false),
	m_drawScale(false),
	m_drawAlpha(false),
	m_shading(true),
	m_backend(""),
	m_xBorder(0),
	m_yBorder(0),
	m_db(NULL),
	m_image(NULL),
	m_xMin(INT_MAX),
	m_xMax(INT_MIN),
	m_zMin(INT_MAX),
	m_zMax(INT_MIN),
	m_yMin(-30000),
	m_yMax(30000),
	m_geomX(-2048),
	m_geomY(-2048),
	m_geomX2(2048),
	m_geomY2(2048),
	m_zoom(1),
	m_scales(SCALE_LEFT | SCALE_TOP)
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

Color TileGenerator::parseColor(const std::string &color)
{
	Color parsed;
	if (color.length() != 7)
		throw std::runtime_error("Color needs to be 7 characters long");
	if (color[0] != '#')
		throw std::runtime_error("Color needs to begin with #");
	unsigned long col = strtoul(color.c_str() + 1, NULL, 16);
	parsed.b = col & 0xff;
	parsed.g = (col >> 8) & 0xff;
	parsed.r = (col >> 16) & 0xff;
	parsed.a = 255;
	return parsed;
}

void TileGenerator::setDrawOrigin(bool drawOrigin)
{
	m_drawOrigin = drawOrigin;
}

void TileGenerator::setDrawPlayers(bool drawPlayers)
{
	m_drawPlayers = drawPlayers;
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
	m_geomX  = round_multiple_nosign(x, 16) / 16;
	m_geomY  = round_multiple_nosign(y, 16) / 16;
	m_geomX2 = round_multiple_nosign(x + w, 16) / 16;
	m_geomY2 = round_multiple_nosign(y + h, 16) / 16;
}

void TileGenerator::setMinY(int y)
{
	m_yMin = y;
}

void TileGenerator::setMaxY(int y)
{
	m_yMax = y;
}

void TileGenerator::parseColorsFile(const std::string &fileName)
{
	ifstream in;
	in.open(fileName.c_str(), ifstream::in);
	if (!in.is_open())
		throw std::runtime_error("Specified colors file could not be found");
	parseColorsStream(in);
}

void TileGenerator::generate(const std::string &input, const std::string &output)
{
	string input_path = input;
	if (input_path[input.length() - 1] != PATH_SEPARATOR) {
		input_path += PATH_SEPARATOR;
	}

	openDb(input_path);
	loadBlocks();
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
	writeImage(output);
	printUnknown();
}

void TileGenerator::parseColorsStream(std::istream &in)
{
	char line[128];
	while (in.good()) {
		in.getline(line, 128);

		for(char *p = line; *p; p++) {
			if(*p != '#')
				continue;
			*p = '\0'; // Cut off at the first #
			break;
		}
		if(strlen(line) == 0)
			continue;

		char name[64];
		unsigned int r, g, b, a, t;
		a = 255;
		t = 0;
		int items = sscanf(line, "%64s %u %u %u %u %u", name, &r, &g, &b, &a, &t);
		if(items < 4) {
			std::cerr << "Failed to parse color entry '" << line << "'" << std::endl;
			continue;
		}
	
		ColorEntry color = ColorEntry(r, g, b, a, t);
		m_colorMap[name] = color;
	}
}

void TileGenerator::openDb(const std::string &input)
{
	std::string backend = m_backend;
	if(backend == "") {
		std::ifstream ifs((input + "/world.mt").c_str());
		if(!ifs.good())
			throw std::runtime_error("Failed to read world.mt");
		backend = get_setting("backend", ifs);
		ifs.close();
	}

	if(backend == "sqlite3")
		m_db = new DBSQLite3(input);
#if USE_POSTGRESQL
	else if(backend == "postgresql")
		m_db = new DBPostgreSQL(input);
#endif
#if USE_LEVELDB
	else if(backend == "leveldb")
		m_db = new DBLevelDB(input);
#endif
#if USE_REDIS
	else if(backend == "redis")
		m_db = new DBRedis(input);
#endif
	else
		throw std::runtime_error(((std::string) "Unknown map backend: ") + backend);
}

void TileGenerator::closeDatabase()
{
	delete m_db;
	m_db = NULL;
}

void TileGenerator::loadBlocks()
{
	std::vector<BlockPos> vec = m_db->getBlockPos();
	for (std::vector<BlockPos>::iterator it = vec.begin(); it != vec.end(); ++it) {
		BlockPos pos = *it;
		// Check that it's in geometry (from --geometry option)
		if (pos.x < m_geomX || pos.x >= m_geomX2 || pos.z < m_geomY || pos.z >= m_geomY2)
			continue;
		// Check that it's between --min-y and --max-y
		if (pos.y * 16 < m_yMin || pos.y * 16 > m_yMax)
			continue;
		// Adjust minimum and maximum positions to the nearest block
		if (pos.x < m_xMin)
			m_xMin = pos.x;
		if (pos.x > m_xMax)
			m_xMax = pos.x;

		if (pos.z < m_zMin)
			m_zMin = pos.z;
		if (pos.z > m_zMax)
			m_zMax = pos.z;
		m_positions.push_back(std::pair<int, int>(pos.x, pos.z));
	}
	m_positions.sort();
	m_positions.unique();
}

void TileGenerator::createImage()
{
	m_mapWidth = (m_xMax - m_xMin + 1) * 16;
	m_mapHeight = (m_zMax - m_zMin + 1) * 16;
	if(m_drawScale) {
		m_xBorder = (m_scales & SCALE_LEFT) ? 40 : 0;
		m_yBorder = (m_scales & SCALE_TOP) ? 40 : 0;
	}
	m_blockPixelAttributes.setWidth(m_mapWidth);

	int image_width, image_height;
	image_width = (m_mapWidth * m_zoom) + m_xBorder;
	image_width += m_drawScale && (m_scales & SCALE_RIGHT) ? 40 : 0;
	image_height = (m_mapHeight * m_zoom) + m_yBorder;
	image_height += m_drawScale && (m_scales & SCALE_BOTTOM) ? 40 : 0;

	if(image_width > 4096 || image_height > 4096)
		std::cerr << "Warning: The width or height of the image to be created exceeds 4096 pixels!"
			<< " (Dimensions: " << image_width << "x" << image_height << ")"
			<< std::endl;
	m_image = new Image(image_width, image_height);
	m_image->drawFilledRect(0, 0, image_width, image_height, m_bgColor); // Background
}

void TileGenerator::renderMap()
{
	std::list<int> zlist = getZValueList();
	for (std::list<int>::iterator zPosition = zlist.begin(); zPosition != zlist.end(); ++zPosition) {
		int zPos = *zPosition;
		std::map<int16_t, BlockList> blocks;
		m_db->getBlocksOnZ(blocks, zPos);
		for (std::list<std::pair<int, int> >::const_iterator position = m_positions.begin(); position != m_positions.end(); ++position) {
			if (position->second != zPos)
				continue;

			for (int i = 0; i < 16; ++i) {
				m_readPixels[i] = 0;
				m_readInfo[i] = 0;
			}
			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 16; j++) {
					m_color[i][j] = m_bgColor; // This will be drawn by renderMapBlockBottom() for y-rows with only 'air', 'ignore' or unknown nodes if --drawalpha is used
					m_color[i][j].a = 0; // ..but set alpha to 0 to tell renderMapBlock() not to use this color to mix a shade
					m_thickness[i][j] = 0;
				}
			}

			int xPos = position->first;
			blocks[xPos].sort();
			const BlockList &blockStack = blocks[xPos];
			for (BlockList::const_iterator it = blockStack.begin(); it != blockStack.end(); ++it) {
				const BlockPos &pos = it->first;
				const unsigned char *data = it->second.c_str();
				size_t length = it->second.length();

				uint8_t version = data[0];
				//uint8_t flags = data[1];

				size_t dataOffset = 0;
				if (version >= 22)
					dataOffset = 4;
				else
					dataOffset = 2;

				ZlibDecompressor decompressor(data, length);
				decompressor.setSeekPos(dataOffset);
				ustring mapData = decompressor.decompress();
				ustring mapMetadata = decompressor.decompress();
				dataOffset = decompressor.seekPos();

				// Skip unused data
				if (version <= 21)
					dataOffset += 2;
				if (version == 23)
					dataOffset += 1;
				if (version == 24) {
					uint8_t ver = data[dataOffset++];
					if (ver == 1) {
						uint16_t num = readU16(data + dataOffset);
						dataOffset += 2;
						dataOffset += 10 * num;
					}
				}

				// Skip unused static objects
				dataOffset++; // Skip static object version
				int staticObjectCount = readU16(data + dataOffset);
				dataOffset += 2;
				for (int i = 0; i < staticObjectCount; ++i) {
					dataOffset += 13;
					uint16_t dataSize = readU16(data + dataOffset);
					dataOffset += dataSize + 2;
				}
				dataOffset += 4; // Skip timestamp

				m_blockAirId = -1;
				m_blockIgnoreId = -1;
				m_nameMap.clear();
				// Read mapping
				if (version >= 22) {
					dataOffset++; // mapping version
					uint16_t numMappings = readU16(data + dataOffset);
					dataOffset += 2;
					for (int i = 0; i < numMappings; ++i) {
						uint16_t nodeId = readU16(data + dataOffset);
						dataOffset += 2;
						uint16_t nameLen = readU16(data + dataOffset);
						dataOffset += 2;
						string name = string(reinterpret_cast<const char *>(data) + dataOffset, nameLen);
						if (name == "air")
							m_blockAirId = nodeId;
						else if (name == "ignore")
							m_blockIgnoreId = nodeId;
						else
							m_nameMap[nodeId] = name;
						dataOffset += nameLen;
					}
					// Skip block if made of only air or ignore nodes
					if (m_nameMap.empty())
						continue;
				}

				// Node timers
				if (version >= 25) {
					dataOffset++;
					uint16_t numTimers = readU16(data + dataOffset);
					dataOffset += 2;
					dataOffset += numTimers * 10;
				}

				renderMapBlock(mapData, pos, version);

				bool allRead = true;
				for (int i = 0; i < 16; ++i) {
					if (m_readPixels[i] != 0xffff)
						allRead = false;
				}
				if (allRead)
					break;
			}
			bool allRead = true;
			for (int i = 0; i < 16; ++i) {
				if (m_readPixels[i] != 0xffff)
					allRead = false;
			}
			if (!allRead)
				renderMapBlockBottom(blockStack.begin()->first);
		}
		if(m_shading)
			renderShading(zPos);
	}
}

void TileGenerator::renderMapBlock(const ustring &mapBlock, const BlockPos &pos, int version)
{
	int xBegin = (pos.x - m_xMin) * 16;
	int zBegin = (m_zMax - pos.z) * 16;
	const unsigned char *mapData = mapBlock.c_str();
	int minY = (pos.y * 16 > m_yMin) ? 0 : m_yMin - pos.y * 16;
	int maxY = (pos.y * 16 < m_yMax) ? 15 : m_yMax - pos.y * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + 15 - z;
		for (int x = 0; x < 16; ++x) {
			if (m_readPixels[z] & (1 << x))
				continue;
			int imageX = xBegin + x;

			for (int y = maxY; y >= minY; --y) {
				int position = x + (y << 4) + (z << 8);
				int content = readBlockContent(mapData, version, position);
				if (content == m_blockAirId || content == m_blockIgnoreId)
					continue;
				NameMap::iterator blockName = m_nameMap.find(content);
				if (blockName == m_nameMap.end()) {
					std::cerr << "Skipping node with invalid ID." << std::endl;
					continue;
				}
				const string &name = blockName->second;
				ColorMap::const_iterator color = m_colorMap.find(name);
				if (color != m_colorMap.end()) {
					const Color c = color->second.to_color();
					if (m_drawAlpha) {
						// mix with previous color (unless first visible time)
						if (m_color[z][x].a == 0)
							m_color[z][x] = c;
						else
							m_color[z][x] = mixColors(m_color[z][x], c);
						if(m_color[z][x].a == 0xff) {
							// color is opaque at this depth (no point continuing)
							setZoomed(imageX, imageY, m_color[z][x]);
							m_readPixels[z] |= (1 << x);
							m_blockPixelAttributes.attribute(15 - z, xBegin + x).thickness = m_thickness[z][x];
						} else {
							// near thickness value to thickness of current node
							m_thickness[z][x] = (m_thickness[z][x] + color->second.t) / 2.0;
							continue;
						}
					} else {
						setZoomed(imageX, imageY, c.noAlpha());
						m_readPixels[z] |= (1 << x);
					}
					if(!(m_readInfo[z] & (1 << x))) {
						m_blockPixelAttributes.attribute(15 - z, xBegin + x).height = pos.y * 16 + y;
						m_readInfo[z] |= (1 << x);
					}
				} else {
					m_unknownNodes.insert(name);
					continue;
				}
				break;
			}
		}
	}
}

void TileGenerator::renderMapBlockBottom(const BlockPos &pos)
{
	int xBegin = (pos.x - m_xMin) * 16;
	int zBegin = (m_zMax - pos.z) * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + 15 - z;
		for (int x = 0; x < 16; ++x) {
			if (m_readPixels[z] & (1 << x))
				continue;
			int imageX = xBegin + x;

			if (m_drawAlpha) {
				// set color in case it wasn't done in renderMapBlock()
				setZoomed(imageX, imageY, m_color[z][x]);
				m_readPixels[z] |= (1 << x);
				m_blockPixelAttributes.attribute(15 - z, xBegin + x).thickness = m_thickness[z][x];
			}
		}
	}
}

void TileGenerator::renderShading(int zPos)
{
	int zBegin = (m_zMax - zPos) * 16;
	for (int z = 0; z < 16; ++z) {
		int imageY = zBegin + z;
		if (imageY >= m_mapHeight)
			continue;
		for (int x = 0; x < m_mapWidth; ++x) {
			if(
				!m_blockPixelAttributes.attribute(z, x).valid_height() ||
				!m_blockPixelAttributes.attribute(z, x - 1).valid_height() ||
				!m_blockPixelAttributes.attribute(z - 1, x).valid_height()
			)
				continue;

			// calculate shadow to apply
			int y = m_blockPixelAttributes.attribute(z, x).height;
			int y1 = m_blockPixelAttributes.attribute(z, x - 1).height;
			int y2 = m_blockPixelAttributes.attribute(z - 1, x).height;
			int d = ((y - y1) + (y - y2)) * 12;
			if (d > 36)
				d = 36;
			// more thickness -> less visible shadows: t=0 -> 100% visible, t=255 -> 0% visible
			if (m_drawAlpha)
				d = d * ((0xFF - m_blockPixelAttributes.attribute(z, x).thickness) / 255.0);

			Color c = m_image->getPixel(getImageX(x), getImageY(imageY));
			c.r = colorSafeBounds(c.r + d);
			c.g = colorSafeBounds(c.g + d);
			c.b = colorSafeBounds(c.b + d);
			setZoomed(x, imageY, c);
		}
	}
	m_blockPixelAttributes.scroll();
}

void TileGenerator::renderScale()
{
	if (m_scales & SCALE_TOP) {
		m_image->drawText(24, 0, "X", m_scaleColor);
		for (int i = (m_xMin / 4) * 4; i <= m_xMax; i += 4) {
			stringstream buf;
			buf << i * 16;

			int xPos = (m_xMin * -16 + i * 16)*m_zoom + m_xBorder;
			m_image->drawText(xPos + 2, 0, buf.str(), m_scaleColor);
			m_image->drawLine(xPos, 0, xPos, m_yBorder - 1, m_scaleColor);
		}
	}

	if (m_scales & SCALE_LEFT) {
		m_image->drawText(2, 24, "Z", m_scaleColor);
		for (int i = (m_zMax / 4) * 4; i >= m_zMin; i -= 4) {
			stringstream buf;
			buf << i * 16;

			int yPos = (m_mapHeight - 1 - (i * 16 - m_zMin * 16))*m_zoom + m_yBorder;
			m_image->drawText(2, yPos, buf.str(), m_scaleColor);
			m_image->drawLine(0, yPos, m_xBorder - 1, yPos, m_scaleColor);
		}
	}

	if (m_scales & SCALE_BOTTOM) {
		for (int i = (m_xMin / 4) * 4; i <= m_xMax; i += 4) {
			stringstream buf;
			buf << i * 16;

			int xPos = (m_xMin * -16 + i * 16)*m_zoom + m_xBorder;
			int yPos = m_yBorder + m_mapHeight*m_zoom;
			m_image->drawText(xPos + 2, yPos, buf.str(), m_scaleColor);
			m_image->drawLine(xPos, yPos, xPos, yPos + 39, m_scaleColor);
		}
	}

	if (m_scales & SCALE_RIGHT) {
		for (int i = (m_zMax / 4) * 4; i >= m_zMin; i -= 4) {
			stringstream buf;
			buf << i * 16;

			int xPos = m_xBorder + m_mapWidth*m_zoom;
			int yPos = (m_mapHeight - 1 - (i * 16 - m_zMin * 16))*m_zoom + m_yBorder;
			m_image->drawText(xPos + 2, yPos, buf.str(), m_scaleColor);
			m_image->drawLine(xPos, yPos, xPos + 39, yPos, m_scaleColor);
		}
	}
}

void TileGenerator::renderOrigin()
{
	int imageX = (-m_xMin * 16)*m_zoom + m_xBorder;
	int imageY = (m_mapHeight - m_zMin * -16)*m_zoom + m_yBorder;
	m_image->drawCircle(imageX, imageY, 12, m_originColor);
}

void TileGenerator::renderPlayers(const std::string &inputPath)
{
	PlayerAttributes players(inputPath);
	for (PlayerAttributes::Players::iterator player = players.begin(); player != players.end(); ++player) {
		int imageX = (player->x / 10 - m_xMin * 16)*m_zoom + m_xBorder;
		int imageY = (m_mapHeight - (player->z / 10 - m_zMin * 16))*m_zoom + m_yBorder;

		m_image->drawCircle(imageX, imageY, 5, m_playerColor);
		m_image->drawText(imageX + 2, imageY + 2, player->name, m_playerColor);
	}
}

inline std::list<int> TileGenerator::getZValueList() const
{
	std::list<int> zlist;
	for (std::list<std::pair<int, int> >::const_iterator position = m_positions.begin(); position != m_positions.end(); ++position)
		zlist.push_back(position->second);
	zlist.sort();
	zlist.unique();
	zlist.reverse();
	return zlist;
}

void TileGenerator::writeImage(const std::string &output)
{
	m_image->save(output);
	delete m_image;
	m_image = NULL;
}

void TileGenerator::printUnknown()
{
	if (m_unknownNodes.size() > 0) {
		std::cerr << "Unknown nodes:" << std::endl;
		for (NameSet::iterator node = m_unknownNodes.begin(); node != m_unknownNodes.end(); ++node) {
			std::cerr << *node << std::endl;
		}
	}
}

inline int TileGenerator::getImageX(int val) const
{
	return (m_zoom*val) + m_xBorder;
}

inline int TileGenerator::getImageY(int val) const
{
	return (m_zoom*val) + m_yBorder;
}

inline void TileGenerator::setZoomed(int x, int y, Color color)
{
	m_image->drawFilledRect(getImageX(x), getImageY(y), m_zoom, m_zoom, color);
}
