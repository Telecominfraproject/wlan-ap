cmake_minimum_required(VERSION 2.8)

PROJECT(mt76-vendor C)
ADD_DEFINITIONS(-Os -Wall --std=gnu99 -g3)

ADD_EXECUTABLE(mt76-vendor main.c csi.c amnt.c capi.c mu.c phy_capa.c)
TARGET_LINK_LIBRARIES(mt76-vendor nl-tiny)

SET(CMAKE_INSTALL_PREFIX /usr)

INSTALL(TARGETS mt76-vendor
	RUNTIME DESTINATION sbin
)
