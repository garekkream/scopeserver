cmake_minimum_required ( VERSION 2.6 )
project (ScopeServer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/)

include_directories(./ scopeproto ${PROTOBUFC_INCLUDE_DIRS} )

add_subdirectory(scopeproto)

file(GLOB SOURCES "*.c")

SET(CMAKE_C_COMPILER "clang")
SET(CMAKE_CXX_COMPILER "clang++")

#execute_process(COMMAND "git describe --tags" __VERSION)
exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR} ARGS "describe --tags" OUTPUT_VARIABLE __VERSION)
add_definitions( -D__VERSION_TAG="${__VERSION}")

find_package( Threads REQUIRED )
find_package( ProtobufC REQUIRED )

SET(CMAKE_C_FLAGS "-O0 -ggdb3 -Wall -Wextra -Wno-missing-braces -Wno-missing-field-initializers -Wformat=2 -Wswitch-enum -Wpointer-arith -Wbad-function-cast -Wstrict-overflow=5 -Wstrict-prototypes -Winline -Wundef -Wnested-externs -Wcast-qual -Wshadow -Wunreachable-code -Wfloat-equal -Wstrict-aliasing=2 -Werror -fno-common -fno-strict-aliasing -fno-omit-frame-pointer -pedantic -Wno-language-extension-token -Wno-gnu-statement-expression -std=gnu99")

add_executable(scope_server ${SOURCES})
target_link_libraries(scope_server scopeproto ${PROTOBUFC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
