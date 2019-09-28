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


#-----------------------------------------------------------------------------
#
# Performs sparse (partial) git checkout
#
#   into ${checkoutDir} from ${url} of ${branch}
#
# List of folders and files to pull can be specified after that.
#-----------------------------------------------------------------------------
function (SparseGitCheckout checkoutDir url branch)
    if(EXISTS ${checkoutDir})
        return()
    endif()

    message("-------------------------------------------------------------------")
    message("sparse git checkout to ${checkoutDir}...")
    message("-------------------------------------------------------------------")

    file(MAKE_DIRECTORY ${checkoutDir})

    set(cmds "git init")
    set(cmds ${cmds} "git remote add -f origin --no-tags -t master ${url}")
    set(cmds ${cmds} "git config core.sparseCheckout true")

    # This command is executed via file WRITE
    # echo <file or folder> >> .git/info/sparse-checkout")

    set(cmds ${cmds} "git pull --depth=1 origin ${branch}")

    # message("In directory: ${checkoutDir}")

    foreach( cmd ${cmds})
        message("- ${cmd}")
        string(REPLACE " " ";" cmdList ${cmd})

        #message("Outfile: ${outFile}")
        #message("Final command: ${cmdList}")

        if(pull IN_LIST cmdList)
            string (REPLACE ";" "\n" FILES "${ARGN}")
            file(WRITE ${checkoutDir}/.git/info/sparse-checkout ${FILES} )
        endif()

        execute_process(
            COMMAND ${cmdList}
            WORKING_DIRECTORY ${checkoutDir}
            RESULT_VARIABLE ret
        )

        if(NOT ret EQUAL "0")
            message("error: previous command failed, see explanation above")
            file(REMOVE_RECURSE ${checkoutDir})
            break()
        endif()
    endforeach()

endfunction()


SparseGitCheckout(${CMAKE_BINARY_DIR}/catch2 https://github.com/catchorg/Catch2.git master single_include)

target_include_directories(test_cppreflect PRIVATE ${CMAKE_BINARY_DIR}/catch2/single_include )

