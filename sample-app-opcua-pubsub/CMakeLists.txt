if (${CROSS_COMPILE})
	set( CMAKE_FIND_ROOT_PATH ${OVERRIDE_CMAKE_ROOT_DIR_HOST} ${OVERRIDE_CMAKE_ROOT_DIR_NATIVE} ${OVERRIDE_CMAKE_ROOT_CROSS_DIR} )
	set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
	set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
	set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
	set( CMAKE_MODULE_PATH ${OVERRIDE_CMAKE_MODULE} )
endif(${CROSS_COMPILE})

cmake_minimum_required(VERSION 3.10)

find_package(open62541 COMPONENTS Events DiscoveryMulticast)

include_directories(/usr/include)
add_definitions(-DUA_ENABLE_PUBSUB_SUBSCRIBER_AMALGAMATION=ON)
add_definitions(-DUA_ENABLE_PUBSUB_SOTXTIME=ON)
add_definitions(-DUA_ENABLE_PUBSUB_ETH_UADP=ON)

add_executable(tutorial_pubsub_publish tutorial_pubsub_publish_sotxtime.c )
target_link_libraries(tutorial_pubsub_publish open62541)

add_executable(tutorial_pubsub_subscribe tutorial_pubsub_subscribe.c )
target_link_libraries(tutorial_pubsub_subscribe open62541)

