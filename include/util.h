#pragma once

#include <string>
#include <fstream>

std::string read_setting(const std::string &name, std::istream &is);

std::string read_setting_default(const std::string &name, std::istream &is,
	const std::string &def);
