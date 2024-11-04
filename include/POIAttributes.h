#pragma once

#include <list>
#include <string>

struct POI
{
	std::string name;
	float x, y, z;
};

class POIAttributes
{
public:
	typedef std::list<POI> POIs;

	POIAttributes(const std::string &worldDir);
	POIs::const_iterator begin() const;
	POIs::const_iterator end() const;

private:
	void readFiles(const std::string &poisPath);
	void readSqlite(const std::string &db_name);

	POIs m_pois;
};
