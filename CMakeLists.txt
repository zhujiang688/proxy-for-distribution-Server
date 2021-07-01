cmake_minimum_required(VERSION 2.8.0)

project(SimpleApiGatewaySystem)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_compile_options(-Wall -std=c++11)

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/log)
link_directories(${PROJECT_SOURCE_DIR}/lib /usr/lib64/mysql /usr/lib64 ${PROJECT_SOURCE_DIR}/src/log)

add_subdirectory(src)
