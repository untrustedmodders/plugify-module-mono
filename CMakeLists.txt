cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()

project(csharp-lang-module VERSION 1.0.0.0  DESCRIPTION "C# Language Module" HOMEPAGE_URL "https://github.com/untrustedmodders/csharp-lang-module" LANGUAGES "CXX")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MONO REQUIRED mono-2)

set(WIZARD_BUILD_DLL ON CACHE INTERNAL "")
set(WIZARD_BUILD_SHARED_LIB ON CACHE INTERNAL "")
add_subdirectory(external/wizard)

#
# Options
#
option(CSHARPLM_BUILD_CORE "Build core and sample plugins with Mono CSC" ON)

#
# C# Language Module for Wizard
#
file(GLOB_RECURSE CSHARPLM_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")
set(CSHARPLM_PCH_FILE "src/pch.h")

add_library(${PROJECT_NAME} SHARED ${CSHARPLM_SOURCES})

include_directories(SYSTEM "${MONO_INCLUDE_DIRS}")
target_link_directories(${PROJECT_NAME} PUBLIC ${MONO_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${MONO_LIBRARIES} wizard)
target_precompile_headers(${PROJECT_NAME} PUBLIC ${CSHARPLM_PCH_FILE})

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_MACRO_NAME CSHARPLM_EXPORT EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/module_export.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/exports)

#
# Wand Core for C# plugins (Building with csc command directly might not work everywhere)
#
find_program(PYTHON_EXECUTABLE NAMES "python")
if(CSHARPLM_BUILD_CORE AND PYTHON_EXECUTABLE)
    function(csc_compilation name out sources args)
        set(MAKE_CMD "csc ${args} -out:${out} ${sources}")
        message("COMMAND: ${MAKE_CMD}")
        execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import subprocess; subprocess.run('${MAKE_CMD}', shell=True)"
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endfunction()

    set(BUILD_FOLDER ${CMAKE_BINARY_DIR}/bin)
    file(MAKE_DIRECTORY ${BUILD_FOLDER}/)

    # Compile Wand
    file(GLOB_RECURSE WAND_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Wand/Wand/src/*.cs")
    string(REPLACE ";" " " WAND_LIST "${WAND_SOURCES}")
    csc_compilation(Wand "${BUILD_FOLDER}/Wand.dll" ${WAND_LIST} "-target:library")

    # Compile Sample Plugins
    file(GLOB_RECURSE PLUGIN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Plugins/Plugins/src/*.cs")
    foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
        get_filename_component(PLUGIN_NAME ${PLUGIN_SOURCE} NAME_WLE)
        set(PLUGIN_PATH "bin/${PLUGIN_NAME}.dll")

        file(MAKE_DIRECTORY ${BUILD_FOLDER}/${PLUGIN_NAME})
        file(MAKE_DIRECTORY ${BUILD_FOLDER}/${PLUGIN_NAME}/bin)

        csc_compilation(${PLUGIN_NAME} "${BUILD_FOLDER}/${PLUGIN_NAME}/${PLUGIN_PATH}" ${PLUGIN_SOURCE} " -target:library -reference:${BUILD_FOLDER}/Wand.dll")
        configure_file("plugin.wplugin.in" "${BUILD_FOLDER}/${PLUGIN_NAME}/${PLUGIN_NAME}.wplugin")
    endforeach()
endif()