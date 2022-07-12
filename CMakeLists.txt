cmake_minimum_required(VERSION 3.7)
project(demo)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb -Wno-error=deprecated-declarations -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-value -Wunused-function")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -Wno-error=deprecated-declarations -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-value -Wunused-function")


include_directories(${PROJECT_SOURCE_DIR}/st)

link_directories(${PROJECT_SOURCE_DIR}/st)

add_executable(example "main.cpp")

target_link_libraries(example
    -Wl,--start-group
    st
    -Wl,--end-group
)