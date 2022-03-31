cmake_minimum_required(VERSION 3.15)

project(Toy)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


find_package(uring)


add_executable(Toy main.cpp)


target_include_directories(Toy PRIVATE ${uring_INCLUDE_DIR})
target_link_libraries(Toy PRIVATE ${uring_LIBRARIES} )

