project(ucrun C)
cmake_minimum_required(VERSION 2.8.12)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3 -O0)
ELSE()
  ADD_DEFINITIONS(-DNDEBUG)
ENDIF()

set(SOURCES main.c ucode.c ubus.c)

add_executable(ucrun ${SOURCES})
target_link_libraries(ucrun ubox)
target_link_libraries(ucrun blobmsg_json)
target_link_libraries(ucrun json-c)
target_link_libraries(ucrun ucode)
target_link_libraries(ucrun ubus)

install(TARGETS ucrun RUNTIME DESTINATION bin)
