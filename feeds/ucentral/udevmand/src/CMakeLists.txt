cmake_minimum_required(VERSION 2.6)

include_directories(/usr/local/include/libnl-tiny)

PROJECT(udevmand C)
INCLUDE(GNUInstallDirs)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(LIBS ubox ubus json-c blobmsg_json uci nl-tiny)

ADD_EXECUTABLE(udevmand main.c mac.c neigh.c ubus.c blob.c netlink.c bridge.c dhcp.c netifd.c ethers.c netdev.c)
TARGET_LINK_LIBRARIES(udevmand ${LIBS})
INSTALL(TARGETS udevmand
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
