cmake_minimum_required(VERSION 2.8.9)
project (skicl)

SET(CMAKE_CXX_FLAGS "-Wall -g -O2 -std=c++17 -I . -Wno-return-type-c-linkage")

option(ENABLE_READLINE "Enable readline shell" OFF)
if (ENABLE_READLINE) 
    set (LIBS ${LIBS} readline) 
    add_definitions (-DENABLE_READLINE)
endif()

add_executable(skicl skicl.cpp skicl.h)
target_link_libraries (skicl dl ${LIBS})

INSTALL(FILES skicl.h DESTINATION include)
INSTALL(PROGRAMS core.tcl DESTINATION $ENV{HOME}/.skicl)
INSTALL(
    TARGETS skicl 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION $ENV{HOME}/.skicl
    ARCHIVE DESTINATION $ENV{HOME}/.skicl)
