cmake_minimum_required(VERSION 3.10)

PROJECT(spotfilter C)

ADD_DEFINITIONS(-Os -Wall -Wno-unknown-warning-option -Wno-array-bounds -Wno-format-truncation -Werror --std=gnu99)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF (NOT DEFINED LIBNL_LIBS)
	include(FindPkgConfig)
	pkg_search_module(LIBNL libnl-3.0 libnl-3 libnl nl-3 nl)
	IF (LIBNL_FOUND)
		include_directories(${LIBNL_INCLUDE_DIRS})
		SET(LIBNL_LIBS ${LIBNL_LIBRARIES})
	ENDIF()
ENDIF()

find_library(bpf NAMES bpf)
ADD_EXECUTABLE(spotfilter main.c bpf.c ubus.c rtnl.c interface.c snoop.c client.c dhcpv4.c icmpv6.c nl80211.c)
TARGET_LINK_LIBRARIES(spotfilter ${bpf} ubox ubus ${LIBNL_LIBS})

INSTALL(TARGETS spotfilter
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
