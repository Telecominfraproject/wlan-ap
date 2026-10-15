cmake_minimum_required(VERSION 3.10)

PROJECT(atenl C)
ADD_DEFINITIONS(-Os -Wall --std=gnu99 -g3)

UNSET(backports_dir CACHE)
FIND_PATH(
	backports_dir
	NAMES "mac80211/uapi/linux"
)
INCLUDE_DIRECTORIES("${backports_dir}/mac80211/uapi")

ADD_EXECUTABLE(atenl main.c eth.c hqa.c nl.c eeprom.c util.c)
TARGET_LINK_LIBRARIES(atenl nl-tiny)

SET(CMAKE_INSTALL_PREFIX /usr)

INSTALL(TARGETS atenl
	RUNTIME DESTINATION sbin
)
