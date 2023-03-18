cmake_minimum_required(VERSION 3.14)
project(ChEst)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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

add_library( CHEST_TOOL ${Chest} ${Loss} ${YAZ} ${Ping} )
target_link_libraries( CHEST_TOOL ${PCAP_LIBRARY} Threads::Threads )

add_executable(launch_chest src/main.cpp)
target_link_libraries(launch_chest PUBLIC  CHEST_TOOL)
