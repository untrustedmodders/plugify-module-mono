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

#
# Plugify
#
set(PLUGIFY_BUILD_SHARED_LIB ON CACHE INTERNAL "")
set(PLUGIFY_BUILD_FUNCTION ON CACHE INTERNAL "")
add_subdirectory(external/plugify)

add_subdirectory(external/dyncall)

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
target_link_libraries(${PROJECT_NAME} PRIVATE ${MONO_LIBS} plugify::plugify plugify::plugify-function asmjit::asmjit glaze::glaze dyncall_s)
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CSHARPLM_PCH_FILE})
include_directories(external/dyncall/)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wextra -Wshadow -Wconversion -Wpedantic -Werror)
endif()

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_MACRO_NAME CSHARPLM_EXPORT EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/module_export.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/exports)
