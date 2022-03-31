cmake_minimum_required(VERSION 3.15)

project(Toy)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_FLAGS "-fpermissive")


add_executable(BoostToy main.cpp)

include(CompileBoost)

target_link_libraries(BoostToy PRIVATE boost_target)


find_package(uring)

target_include_directories(BoostToy PRIVATE ${uring_INCLUDE_DIR})
target_link_libraries(BoostToy PRIVATE ${uring_LIBRARIES} boost_target)
target_compile_definitions(BoostToy PRIVATE BOOST_ASIO_HAS_IO_URING=1 BOOST_ASIO_DISABLE_EPOLL=1)