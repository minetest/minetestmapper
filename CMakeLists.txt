project(minetestmapper CXX)
cmake_minimum_required(VERSION 2.6)
cmake_policy(SET CMP0003 NEW)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}")

# Stuff & Paths

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g2 -Wall")

if(WIN32)
	set(SHAREDIR ".")
	set(BINDIR ".")
	set(DOCDIR ".")
else()
	set(SHAREDIR "${CMAKE_INSTALL_PREFIX}/share/minetest") # an extra dir. for just one file doesn't seem useful
	set(BINDIR "${CMAKE_INSTALL_PREFIX}/bin")
	set(DOCDIR "${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}")
endif()

set(CUSTOM_SHAREDIR "" CACHE STRING "Directory to install data files into")
if(NOT CUSTOM_SHAREDIR STREQUAL "")
        set(SHAREDIR "${CUSTOM_SHAREDIR}")
        message(STATUS "Using SHAREDIR=${SHAREDIR}")
endif()

set(CUSTOM_BINDIR "" CACHE STRING "Directory to install binaries into")
if(NOT CUSTOM_BINDIR STREQUAL "")
        set(BINDIR "${CUSTOM_BINDIR}")
        message(STATUS "Using BINDIR=${BINDIR}")
endif()

set(CUSTOM_DOCDIR "" CACHE STRING "Directory to install documentation into")
if(NOT CUSTOM_DOCDIR STREQUAL "")
        set(DOCDIR "${CUSTOM_DOCDIR}")
        message(STATUS "Using DOCDIR=${DOCDIR}")
endif()


# Libraries: gd

find_library(LIBGD_LIBRARY gd)
find_path(LIBGD_INCLUDE_DIR gd.h)
message (STATUS "libgd library: ${LIBGD_LIBRARY}")
message (STATUS "libgd headers: ${LIBGD_INCLUDE_DIR}")
if(NOT LIBGD_LIBRARY OR NOT LIBGD_INCLUDE_DIR)
	message(FATAL_ERROR "libgd not found!")
endif(NOT LIBGD_LIBRARY OR NOT LIBGD_INCLUDE_DIR)

# Libraries: zlib

find_library(ZLIB_LIBRARY z)
find_path(ZLIB_INCLUDE_DIR zlib.h)
message (STATUS "zlib library: ${ZLIB_LIBRARY}")
message (STATUS "zlib headers: ${ZLIB_INCLUDE_DIR}")
if(NOT ZLIB_LIBRARY OR NOT ZLIB_INCLUDE_DIR)
	message(FATAL_ERROR "zlib not found!")
endif(NOT ZLIB_LIBRARY OR NOT ZLIB_INCLUDE_DIR)

find_package(PkgConfig)
include(FindPackageHandleStandardArgs)

# Libraries: sqlite3

find_library(SQLITE3_LIBRARY sqlite3)
find_path(SQLITE3_INCLUDE_DIR zlib.h)
message (STATUS "sqlite3 library: ${SQLITE3_LIBRARY}")
message (STATUS "sqlite3 headers: ${SQLITE3_INCLUDE_DIR}")
if(NOT SQLITE3_LIBRARY OR NOT SQLITE3_INCLUDE_DIR)
	message(FATAL_ERROR "sqlite3 not found!")
endif(NOT SQLITE3_LIBRARY OR NOT SQLITE3_INCLUDE_DIR)

# Libraries: leveldb

set(USE_LEVELDB 0)

OPTION(ENABLE_LEVELDB "Enable LevelDB backend")

if(ENABLE_LEVELDB)
	find_library(LEVELDB_LIBRARY leveldb)
	find_path(LEVELDB_INCLUDE_DIR db.h PATH_SUFFIXES leveldb)
	message (STATUS "LevelDB library: ${LEVELDB_LIBRARY}")
	message (STATUS "LevelDB headers: ${LEVELDB_INCLUDE_DIR}")
	if(LEVELDB_LIBRARY AND LEVELDB_INCLUDE_DIR)
		set(USE_LEVELDB 1)
		message(STATUS "LevelDB backend enabled")
		include_directories(${LEVELDB_INCLUDE_DIR})
	else(LEVELDB_LIBRARY AND LEVELDB_INCLUDE_DIR)
		set(USE_LEVELDB 0)
		message(STATUS "LevelDB not found!")
	endif(LEVELDB_LIBRARY AND LEVELDB_INCLUDE_DIR)
endif(ENABLE_LEVELDB)

# Libraries: redis

set(USE_REDIS 0)

OPTION(ENABLE_REDIS "Enable redis backend")

if(ENABLE_REDIS)
	find_library(REDIS_LIBRARY hiredis)
	find_path(REDIS_INCLUDE_DIR hiredis.h PATH_SUFFIXES hiredis)
	message (STATUS "redis library: ${REDIS_LIBRARY}")
	message (STATUS "redis headers: ${REDIS_INCLUDE_DIR}")
	if(REDIS_LIBRARY AND REDIS_INCLUDE_DIR)
		set(USE_REDIS 1)
		message(STATUS "redis backend enabled")
		include_directories(${REDIS_INCLUDE_DIR})
	else(REDIS_LIBRARY AND REDIS_INCLUDE_DIR)
		set(USE_REDIS 0)
		message(STATUS "redis not found!")
	endif(REDIS_LIBRARY AND REDIS_INCLUDE_DIR)
endif(ENABLE_REDIS)

# Compiling & Linking

include_directories(
	"${PROJECT_BINARY_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}"
	${SQLITE3_INCLUDE_DIR}
	${LIBGD_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
)

configure_file(
	"${PROJECT_SOURCE_DIR}/cmake_config.h.in"
	"${PROJECT_BINARY_DIR}/cmake_config.h"
)
add_definitions ( -DUSE_CMAKE_CONFIG_H )

set(mapper_SRCS
	PixelAttributes.cpp
	PlayerAttributes.cpp
	TileGenerator.cpp
	ZlibDecompressor.cpp
	mapper.cpp
	util.cpp
	db-sqlite3.cpp
)

if(USE_LEVELDB)
	set(mapper_SRCS ${mapper_SRCS} db-leveldb.cpp)
endif(USE_LEVELDB)

if(USE_REDIS)
	set(mapper_SRCS ${mapper_SRCS} db-redis.cpp)
endif(USE_REDIS)

add_executable(minetestmapper
	${mapper_SRCS}
)

target_link_libraries(
	minetestmapper
	${SQLITE3_LIBRARY}
	${LEVELDB_LIBRARY}
	${REDIS_LIBRARY}
	${LIBGD_LIBRARY}
	${ZLIB_LIBRARY}
)

# Installing & Packaging

install(TARGETS "${PROJECT_NAME}" DESTINATION "${BINDIR}")
install(FILES "AUTHORS" DESTINATION "${DOCDIR}")
install(FILES "COPYING" DESTINATION "${DOCDIR}")
install(FILES "README.rst" DESTINATION "${DOCDIR}")
install(FILES "colors.txt" DESTINATION "${SHAREDIR}")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Overview mapper for Minetest")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VENDOR "celeron55")
set(CPACK_PACKAGE_CONTACT "Perttu Ahola <celeron55@gmail.com>")

if(WIN32)
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-win32")
	set(CPACK_GENERATOR ZIP)
else()
	set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${VERSION_STRING}-linux")
	set(CPACK_GENERATOR TGZ)
	set(CPACK_SOURCE_GENERATOR TGZ)
endif()

include(CPack)
