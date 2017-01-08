#if MSDOS || __OS2__ || __NT__ || _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#define BLOCK_SIZE 16

#ifdef USE_CMAKE_CONFIG_H
#include "cmake_config.h"
#else
#define USE_POSTGRESQL 0
#define USE_LEVELDB 0
#define USE_REDIS 0

#define SHAREDIR "/usr/share/minetest"
#endif
