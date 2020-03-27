#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <fstream>

std::string read_setting(const std::string &name, std::istream &is);

inline std::string read_setting_default(const std::string &name, std::istream &is, const std::string &def)
{
	try {
		return read_setting(name, is);
	} catch(const std::runtime_error &e) {
		return def;
	}
}

#endif // UTIL_H
