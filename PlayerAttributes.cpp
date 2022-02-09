#include <fstream>
#include <sstream>
#include <stdexcept>
#include <dirent.h>
#include <unistd.h> // for usleep
#include <sqlite3.h>

#include "config.h"
#include "PlayerAttributes.h"
#include "util.h"

PlayerAttributes::PlayerAttributes(const std::string &worldDir)
{
	std::ifstream ifs(worldDir + "world.mt");
	if (!ifs.good())
		throw std::runtime_error("Failed to read world.mt");
	std::string backend = read_setting_default("player_backend", ifs, "files");
	ifs.close();

	if (backend == "files")
		readFiles(worldDir + "players");
	else if (backend == "sqlite3")
		readSqlite(worldDir + "players.sqlite");
	else
		throw std::runtime_error(std::string("Unknown player backend: ") + backend);
}

void PlayerAttributes::readFiles(const std::string &playersPath)
{
	DIR *dir;
	dir = opendir (playersPath.c_str());
	if (!dir)
		return;

	struct dirent *ent;
	while ((ent = readdir (dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;

		std::ifstream in(playersPath + PATH_SEPARATOR + ent->d_name);
		if (!in.good())
			continue;

		std::string name, position;
		name = read_setting("name", in);
		in.seekg(0);
		position = read_setting("position", in);

		Player player;
		std::istringstream iss(position);
		char tmp;
		iss >> tmp; // '('
		iss >> player.x;
		iss >> tmp; // ','
		iss >> player.y;
		iss >> tmp; // ','
		iss >> player.z;
		iss >> tmp; // ')'
		if (tmp != ')')
			continue;
		player.name = name;

		player.x /= 10.0f;
		player.y /= 10.0f;
		player.z /= 10.0f;

		m_players.push_back(player);
	}

	closedir(dir);
}

/**********/

#define SQLRES(f, good) \
	result = (sqlite3_##f); \
	if (result != good) { \
		throw std::runtime_error(sqlite3_errmsg(db));\
	}
#define SQLOK(f) SQLRES(f, SQLITE_OK)

void PlayerAttributes::readSqlite(const std::string &db_name)
{
	int result;
	sqlite3 *db;
	sqlite3_stmt *stmt_get_player_pos;

	SQLOK(open_v2(db_name.c_str(), &db, SQLITE_OPEN_READONLY |
			SQLITE_OPEN_PRIVATECACHE, 0))

	SQLOK(prepare_v2(db,
			"SELECT name, posX, posY, posZ FROM player",
		-1, &stmt_get_player_pos, NULL))

	while ((result = sqlite3_step(stmt_get_player_pos)) != SQLITE_DONE) {
		if (result == SQLITE_BUSY) { // Wait some time and try again
			usleep(10000);
		} else if (result != SQLITE_ROW) {
			throw std::runtime_error(sqlite3_errmsg(db));
		}

		Player player;
		const unsigned char *name_ = sqlite3_column_text(stmt_get_player_pos, 0);
		player.name = reinterpret_cast<const char*>(name_);
		player.x = sqlite3_column_double(stmt_get_player_pos, 1);
		player.y = sqlite3_column_double(stmt_get_player_pos, 2);
		player.z = sqlite3_column_double(stmt_get_player_pos, 3);

		player.x /= 10.0f;
		player.y /= 10.0f;
		player.z /= 10.0f;

		m_players.push_back(player);
	}

	sqlite3_finalize(stmt_get_player_pos);
	sqlite3_close(db);
}

/**********/

PlayerAttributes::Players::const_iterator PlayerAttributes::begin() const
{
	return m_players.cbegin();
}

PlayerAttributes::Players::const_iterator PlayerAttributes::end() const
{
	return m_players.cend();
}

