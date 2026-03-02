cmake_minimum_required(VERSION 3.10)

project(openwifi-tools C)
include(GNUInstallDirs)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Comprehensive warning flags (cross-compilation friendly)
set(WARNING_FLAGS
    -Wall
    -Wextra
    -Werror
    -Wformat=2
    -Wformat-security
    -Wnull-dereference
    -Warray-bounds=2
    -Wimplicit-fallthrough=3
    -Wshift-overflow=2
    -Wcast-qual
    -Wstringop-overflow=4
    -Wlogical-op
    -Wduplicated-cond
    -Wduplicated-branches
    -Wformat-overflow=2
    -Wformat-truncation=2
    -Wmissing-declarations
    -Wredundant-decls
    -Wshadow
    -Wstrict-overflow=4
    -Wundef
    -Wstrict-prototypes
    -Wswitch-default
    -Wbad-function-cast
    -Wwrite-strings
    -Wjump-misses-init
    -Wold-style-definition
    -Wmissing-prototypes
)

# Security hardening flags (cross-compilation friendly)
set(SECURITY_FLAGS
    -fstack-protector-strong
)

# Linker security flags (cross-compilation friendly)
set(SECURITY_LINKER_FLAGS
    -Wl,-z,relro
    -Wl,-z,now
)

# Debug flags
set(DEBUG_FLAGS
    -g3
    -ggdb
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
)

# Release optimization flags
set(RELEASE_FLAGS
    -O2
    -flto
    -ffunction-sections
    -fdata-sections
)

set(RELEASE_LINKER_FLAGS
    -Wl,--gc-sections
    -flto
)

# Apply flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(${WARNING_FLAGS} ${SECURITY_FLAGS} ${DEBUG_FLAGS})
    add_link_options(${SECURITY_LINKER_FLAGS})
else()
    add_compile_options(${WARNING_FLAGS} ${SECURITY_FLAGS} ${RELEASE_FLAGS})
    add_link_options(${SECURITY_LINKER_FLAGS} ${RELEASE_LINKER_FLAGS})
endif()

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

# Define all targets
set(TARGETS firstcontact dhcpdiscover dnsprobe radiusprobe)

# firstcontact executable
add_executable(firstcontact firstcontact.c)
target_link_libraries(firstcontact PRIVATE curl crypto ssl ubox)

# dhcpdiscover executable  
add_executable(dhcpdiscover dhcpdiscover.c)

# dnsprobe executable
add_executable(dnsprobe dnsprobe.c)
target_link_libraries(dnsprobe PRIVATE ubox resolv)

# radiusprobe executable
add_executable(radiusprobe radiusprobe.c)
target_link_libraries(radiusprobe PRIVATE radcli)

# Install all targets
install(TARGETS ${TARGETS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
    COMPONENT Runtime
)

