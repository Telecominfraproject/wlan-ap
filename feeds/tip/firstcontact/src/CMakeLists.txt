cmake_minimum_required(VERSION 2.6)

PROJECT(digicert C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

ADD_EXECUTABLE(digicert digicert.c)
TARGET_LINK_LIBRARIES(digicert curl crypto ssl ubox)
INSTALL(TARGETS digicert
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
