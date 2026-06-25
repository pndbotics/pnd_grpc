cmake_minimum_required(VERSION 3.10)
project(Client)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(nlohmann_json REQUIRED)


get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ROOT_DIR}/Client/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ROOT_DIR}/Client/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ROOT_DIR}/Client/lib")

add_subdirectory(src)