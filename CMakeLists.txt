cmake_minimum_required(VERSION 3.12)

project(cppreflect)

add_executable(test_cppreflect 
    pugixml/pugiconfig.hpp
    pugixml/pugixml.cpp
    pugixml/pugixml.hpp
    pugixml/pugixml.natvis
    cppreflect/typetraits.h
    cppreflect/typetraits.cpp
    cppreflect/macrohelpers.h
    cppreflect/enumreflect.h
    cppreflect/cppreflect.h
    cppreflect/cppreflect.cpp
    test_cppreflect.cpp
)

target_include_directories(test_cppreflect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
target_compile_features(test_cppreflect PRIVATE cxx_std_17)
target_compile_definitions(test_cppreflect PRIVATE UNICODE;_UNICODE)

if(NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'$ORIGIN' -Wl,--no-undefined")
    target_compile_options(test_cppreflect PRIVATE -fpermissive -Wno-invalid-offsetof)
    #target_compile_options(test_cppreflect PRIVATE -E)
endif()

if(NOT DEFINED ENV{TRAVIS})
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
endif()


