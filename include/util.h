#pragma once

#include <string>
#include <iostream>

template<typename T>
static inline T mymax(T a, T b)
{
	return (a > b) ? a : b;
}

template<typename T>
static inline T mymin(T a, T b)
{
	return (a > b) ? b : a;
}

std::string read_setting(const std::string &name, std::istream &is);

std::string read_setting_default(const std::string &name, std::istream &is,
	const std::string &def);
