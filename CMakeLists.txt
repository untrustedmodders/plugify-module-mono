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

add_subdirectory(external/plugify)

#
# Options
#
option(CSHARPLM_BUILD_CORE "Build core and sample plugins with Mono CSC" ON)

#
# Mono
#
set(MONO_LIBRARY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/mono/lib)
#set(MONO_LIBS eglib libgcmonosgen libmini-sgen libmonoruntime-sgen libmono-static-sgen libmonoutils mono-2.0-sgen MonoPosixHelper)
set(MONO_LIBS libmono-static-sgen)
foreach(library ${MONO_LIBS})
    add_library(${library} STATIC IMPORTED)
    set_target_properties(${library} PROPERTIES
            IMPORTED_LOCATION_DEBUG "${MONO_LIBRARY_PATH}/Debug/${library}${CMAKE_STATIC_LIBRARY_SUFFIX}"
            IMPORTED_LOCATION_RELEASE "${MONO_LIBRARY_PATH}/Release/${library}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endforeach()
if(WIN32)
    set(MONO_LIBS ${MONO_LIBS} ws2_32.lib winmm.lib version.lib bcrypt.lib)
endif()

#
# C# Language Module for Plugify
#
file(GLOB_RECURSE CSHARPLM_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")
set(CSHARPLM_PCH_FILE "src/pch.h")

add_library(${PROJECT_NAME} SHARED ${CSHARPLM_SOURCES})

include_directories(external/mono/include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${MONO_LIBS} plugify glaze::glaze asmjit::asmjit $<TARGET_OBJECTS:plugify-function>)
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CSHARPLM_PCH_FILE})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wextra -Wshadow -Wconversion -Wpedantic -Werror)
endif()

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_MACRO_NAME CSHARPLM_EXPORT EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/module_export.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/exports)

#
# Plugify Core for C# plugins (Building with csc command directly might not work everywhere)
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

    # Compile Plugify
    file(GLOB_RECURSE PLUGIFY_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Plugify/Plugify/src/*.cs")
    string(REPLACE ";" " " PLUGIFY_LIST "${PLUGIFY_SOURCES}")
    csc_compilation(Plugify "${BUILD_FOLDER}/Plugify.dll" ${PLUGIFY_LIST} "-target:library")

    # Compile Sample Plugins
    file(GLOB_RECURSE PLUGIN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "script-core/Plugins/Plugins/src/*.cs")
    foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
        get_filename_component(PLUGIN_NAME ${PLUGIN_SOURCE} NAME_WLE)
        set(PLUGIN_PATH "bin/${PLUGIN_NAME}.dll")

        file(MAKE_DIRECTORY ${BUILD_FOLDER}/${PLUGIN_NAME})
        file(MAKE_DIRECTORY ${BUILD_FOLDER}/${PLUGIN_NAME}/bin)

        csc_compilation(${PLUGIN_NAME} "${BUILD_FOLDER}/${PLUGIN_NAME}/${PLUGIN_PATH}" ${PLUGIN_SOURCE} " -target:library -reference:${BUILD_FOLDER}/Plugify.dll")
        configure_file("plugin.wplugin.in" "${BUILD_FOLDER}/${PLUGIN_NAME}/${PLUGIN_NAME}.wplugin")
    endforeach()
endif()