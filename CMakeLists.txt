CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT("bms_daly")

set (CMAKE_CXX_FLAGS "-O2 --std=c++0x ${CMAKE_CXX_FLAGS}")

file(GLOB SOURCES *.cpp)
ADD_EXECUTABLE(smart_bms ${SOURCES})
target_link_libraries(smart_bms -lpthread)
