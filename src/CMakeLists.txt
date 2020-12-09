cmake_minimum_required(VERSION 2.8.9)
project (quile)

SET(CMAKE_CXX_FLAGS "-Wall -g -O2 -std=c++17 -I . -Wno-return-type-c-linkage")

option(ENABLE_READLINE "Enable readline shell" OFF)
if (ENABLE_READLINE) 
    set (LIBS ${LIBS} readline) 
    add_definitions (-DENABLE_READLINE)
endif()

add_executable(quile quile.cpp core.h BPF.h FFT.h numeric.h system.h TwoPoles.h WavFile.h)
target_link_libraries (quile dl ${LIBS})

INSTALL(PROGRAMS stdlib.tcl DESTINATION $ENV{HOME}/.quile)
INSTALL(
    TARGETS quile 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION $ENV{HOME}/.quile
    ARCHIVE DESTINATION $ENV{HOME}/.quile)
