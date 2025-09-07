cmake_minimum_required(VERSION 2.6)

PROJECT(openwifi-tools C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

ADD_EXECUTABLE(firstcontact firstcontact.c)
TARGET_LINK_LIBRARIES(firstcontact curl crypto ssl ubox)
INSTALL(TARGETS firstcontact
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

ADD_EXECUTABLE(dhcpdiscover dhcpdiscover.c)
INSTALL(TARGETS dhcpdiscover
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

ADD_EXECUTABLE(dnsprobe dnsprobe.c)
TARGET_LINK_LIBRARIES(dnsprobe ubox resolv)
INSTALL(TARGETS dnsprobe
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

ADD_EXECUTABLE(radiusprobe radiusprobe.c)
TARGET_LINK_LIBRARIES(radiusprobe radcli)
INSTALL(TARGETS radiusprobe
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

ADD_EXECUTABLE(ip-collide ip-collide.c)
TARGET_LINK_LIBRARIES(ip-collide ubox)
INSTALL(TARGETS ip-collide
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
