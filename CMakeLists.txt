cmake_minimum_required(VERSION 3.14)
project(ChEst)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})    # to find *.bp.h files

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include(FindPCAP.cmake)

set(YAZ src/abet/yaz/yaz.h
        src/abet/yaz/yaz.cc
        src/abet/yaz/yaz_send.cc
        src/abet/yaz/yaz_recv.cc
)

set(Ping src/ping/pinger.h
         src/ping/pinger.cpp
)

set(Loss src/loss/loss.h
         src/loss/loss.cpp
)

set(Chest src/chest.h
          src/chest.cpp
)


option(USER_TEST "Compile test.cpp file only" OFF)
if(USER_TEST)
    #protobuf_generate_cpp(PROTO_SRC PROTO_HEADER test.proto)
    #add_library(proto ${PROTO_HEADER} ${PROTO_SRC})
    add_executable(test test.cpp)
    target_link_libraries(test ${YAML_CPP_LIBRARIES})
    #target_link_libraries(test proto ${PROTOBUF_LIBRARY})
else()
    protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ${CMAKE_SOURCE_DIR}/src/abet/yaz/PsVec.proto)
    add_library(proto ${PROTO_HEADER} ${PROTO_SRC})

    #add_library( YAZ_lib ${YAZ} )
    #target_link_libraries( YAZ_lib proto ${PROTOBUF_LIBRARY} ${PCAP_LIBRARY} )

    add_library( CHEST_TOOL ${Chest} ${YAZ} ${Loss} ${Ping} )
    target_link_libraries( CHEST_TOOL ${PCAP_LIBRARY} Threads::Threads proto  ${PROTOBUF_LIBRARY} ${YAML_CPP_LIBRARIES} )

    add_executable(launch_chest src/main.cpp)
    target_link_libraries(launch_chest PUBLIC CHEST_TOOL)
endif()

