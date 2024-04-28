#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <dirent.h>
#include <unistd.h> // for usleep
#include <sqlite3.h>

#include "config.h"
#include "POIAttributes.h"
#include "util.h"

POIAttributes::POIAttributes(const std::string &worldDir)
{
	std::ifstream ifs(worldDir + "world.mt");
	if (!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string backend = read_setting_default("mod_storage_backend", ifs, "sqlite3");
	ifs.close();

	if (backend == "sqlite3")
		readSqlite(worldDir + "mod_storage.sqlite");
	else
		throw std::runtime_error(std::string("Unknown poi backend: ") + backend);
}

/**********/

#define SQLRES(f, good) \
	result = (sqlite3_##f); \
	if (result != good) { \
		throw std::runtime_error(sqlite3_errmsg(db));\
	}
#define SQLOK(f) SQLRES(f, SQLITE_OK)

void POIAttributes::readSqlite(const std::string &db_name)
{
	int result;
	sqlite3 *db;
	sqlite3_stmt *stmt_get_poi_pos;

	SQLOK(open_v2(db_name.c_str(), &db, SQLITE_OPEN_READONLY |
			SQLITE_OPEN_PRIVATECACHE, 0))

	SQLOK(prepare_v2(db,
			"SELECT key, value FROM entries WHERE modname = 'poi'",
		-1, &stmt_get_poi_pos, NULL))

	while ((result = sqlite3_step(stmt_get_poi_pos)) != SQLITE_DONE) {
		if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		}

		POI poi;
		const unsigned char *name_ = sqlite3_column_text(stmt_get_poi_pos, 0);
		poi.name = reinterpret_cast<const char*>(name_);
		const char *pos_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt_get_poi_pos, 1));
		float x, y, z;
		int items = sscanf(pos_, "(%f,%f,%f)", &x, &y, &z);
		if (items != 3) {
		   std::cerr << "Failed to parse POI position '" << pos_ << "'" << std::endl;
		   return;
		}
		poi.x = x;
		poi.y = y;
		poi.z = z;

		m_pois.push_back(poi);
	}

	sqlite3_finalize(stmt_get_poi_pos);
	sqlite3_close(db);
}

/**********/

POIAttributes::POIs::const_iterator POIAttributes::begin() const
{
	return m_pois.cbegin();
}

POIAttributes::POIs::const_iterator POIAttributes::end() const
{
	return m_pois.cend();
}
