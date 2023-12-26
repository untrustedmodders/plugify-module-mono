cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(csharp-lang-module)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Speed up building on MinGW
if(MINGW)
    add_link_options(-fuse-ld=lld)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MONO REQUIRED mono-2)

#
# C# Language Module for Wizard
#
file(GLOB_RECURSE CSHARPLM_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")
set(CSHARPLM_PCH_FILE "src/pch.h")

add_library(${PROJECT_NAME} SHARED ${CSHARPLM_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "external/wizard/include")
include_directories(SYSTEM "${MONO_INCLUDE_DIRS}")
target_link_directories(${PROJECT_NAME} PUBLIC ${MONO_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${MONO_LIBRARIES})
target_precompile_headers(${PROJECT_NAME} PUBLIC ${CSHARPLM_PCH_FILE})

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_MACRO_NAME CSHARPLM_EXPORT EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/module_export.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/exports)

#
# Wand Core for C# plugin
#
find_program(PYTHON_EXECUTABLE NAMES "python")
if(PYTHON_EXECUTABLE)
    function(csc_compilation name out sources args)
        set(MAKE_CMD "csc ${args} -out:${out} ${sources}")
        message("COMMAND: ${MAKE_CMD}")
        #execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import subprocess; subprocess.run('${MAKE_CMD}', shell=True)"
       #         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD
                COMMAND ${PYTHON_EXECUTABLE} -c "import subprocess; subprocess.run('${MAKE_CMD}', shell=True)"
                COMMENT "Executing post-build step"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                VERBATIM
        )
    endfunction()

    # Compile Wand
    file(GLOB_RECURSE WAND_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Wand/Wand/src/*.cs")
    string(REPLACE ";" " " WAND_LIST "${WAND_SOURCES}")
    csc_compilation(Wand "bin/Wand.dll" ${WAND_LIST} "-target:library")

    # Compile Sample Plugins
    file(GLOB_RECURSE PLUGIN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Plugins/Plugins/src/*.cs")
    foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
        get_filename_component(PLUGIN_NAME ${PLUGIN_SOURCE} NAME_WLE)
        csc_compilation(${PLUGIN_NAME} "bin/${PLUGIN_NAME}.dll" ${PLUGIN_SOURCE} " -target:library -reference:bin/Wand.dll")
    endforeach()
endif()