#if MSDOS || __OS2__ || __NT__ || _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#ifdef USE_CMAKE_CONFIG_H
#include "cmake_config.h"
#else
#error missing config
#endif
