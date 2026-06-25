cmake_minimum_required(VERSION 3.10)
project(AdamCommand)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

set(CLIENT_SOURCES
    adam_command.cpp
    ../../include/adam_control.pb.cc
    ../../include/adam_control.grpc.pb.cc
)

add_library(adam_command ${CLIENT_SOURCES})
target_include_directories(adam_command PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(adam_command
    PUBLIC
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    ${CMAKE_THREAD_LIBS_INIT}
)

add_executable(adam_command_client adam_command_cli.cpp)
target_include_directories(adam_command_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(adam_command_client
    PRIVATE
    adam_command
    nlohmann_json::nlohmann_json
)
