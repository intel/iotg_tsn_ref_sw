if (${CROSS_COMPILE})
	set( CMAKE_FIND_ROOT_PATH ${OVERRIDE_CMAKE_ROOT_DIR_HOST} ${OVERRIDE_CMAKE_ROOT_DIR_NATIVE} ${OVERRIDE_CMAKE_ROOT_CROSS_DIR} )
	set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
	set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
	set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
	set( CMAKE_MODULE_PATH ${OVERRIDE_CMAKE_MODULE} )
endif(${CROSS_COMPILE})

cmake_minimum_required(VERSION 3.10)

# Debug to have debug symbols
# Release for optimisations and no debug symbols
# RelWithDebInfo for optimisation and debug symbols
set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -D_FORTIFY_SOURCE=2 \
		-fstack-protector-strong -fPIE -fPIC \
		-Wformat -Wformat-security -Wall -Wextra -Wno-parentheses \
		-Wno-missing-field-initializers -z noexecstack -z relro -z now -pie")

add_executable(opcua-server multicallback_server.c opcua_common.c
		opcua_publish.c json_helper.c opcua_subscribe.c
		opcua_custom.c opcua_datasource.c)

target_link_libraries(opcua-server open62541 pthread elf rt json-c)
