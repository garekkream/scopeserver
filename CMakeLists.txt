cmake_minimum_required ( VERSION 2.6 )
project (ScopeServer)

include_directories(./)

file(GLOB SOURCES "*.c")

add_executable(scope_server ${SOURCES})
