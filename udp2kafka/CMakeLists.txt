cmake_minimum_required(VERSION 3.6)
project(udp2kafka)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(LibRDKafka REQUIRED)

include_directories(${LibRDKafka_INCLUDE_DIR})
link_libraries(${LibRDKafka_LIBRARIES} ${LibRDKafka_C_LIBRARIES})

set(SOURCE_FILES main.cpp)
add_executable(udp2kafka ${SOURCE_FILES})

target_link_libraries(udp2kafka ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(udp2kafka ${Boost_LIBRARIES})
target_link_libraries(udp2kafka ${CONAN_LIBS})