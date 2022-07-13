cmake_minimum_required(VERSION 3.7)

include_directories(${PROJECT_SOURCE_DIR}/st ${PROJECT_SOURCE_DIR})
link_directories(${PROJECT_SOURCE_DIR}/st)

add_executable(example1 "example1.cpp")

target_link_libraries(example1
    st
    pthread
)

add_executable(example2 "example2.cpp")

target_link_libraries(example2
    st
    pthread
)