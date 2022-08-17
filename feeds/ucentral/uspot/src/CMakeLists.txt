cmake_minimum_required(VERSION 2.6)

PROJECT(radius-client C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

ADD_EXECUTABLE(radius-client radius.c)
TARGET_LINK_LIBRARIES(radius-client radcli ubox blobmsg_json)
INSTALL(TARGETS radius-client
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

ADD_LIBRARY(uam SHARED uam.c)
TARGET_LINK_LIBRARIES(uam ubox)
INSTALL(TARGETS uam LIBRARY DESTINATION lib)
