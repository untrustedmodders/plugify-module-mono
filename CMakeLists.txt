cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0092)
	cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()


project(csharp-lang-module VERSION 1.0.0.0  DESCRIPTION "C# Language Module" HOMEPAGE_URL "https://github.com/untrustedmodders/csharp-lang-module" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(UNIX)
	include(CheckCCompilerFlag)
	check_c_compiler_flag("-fPIC" COMPILER_SUPPORTS_FPIC)
	if(COMPILER_SUPPORTS_FPIC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	endif()
endif()

#
# Plugify
#
set(PLUGIFY_BUILD_SHARED_LIB ON CACHE INTERNAL "")
set(PLUGIFY_BUILD_FUNCTION ON CACHE INTERNAL "")
add_subdirectory(external/plugify)

#
# Dyncall
#
add_subdirectory(external/dyncall)

#
# Mono
#
if(WIN32)
	set(CSHARPLM_PLATFORM "windows")
elseif(APPLE)
	set(CSHARPLM_PLATFORM "apple")
elseif(UNIX)
	set(CSHARPLM_PLATFORM "linux")
endif()

if(WIN32)
	set(MONO_LIB libmono-static-sgen)
else()
	set(MONO_LIB libmonosgen-2.0)
	set(MONO_NATIVE_LIB libmono-native)
endif()

set(MONO_LIBRARY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/mono/lib/${CSHARPLM_PLATFORM})

if(WIN32)
	add_library(${MONO_LIB} STATIC IMPORTED)
	set_target_properties(${MONO_LIB} PROPERTIES
			IMPORTED_LOCATION_DEBUG "${MONO_LIBRARY_PATH}/Debug/${MONO_LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
			IMPORTED_LOCATION_RELEASE "${MONO_LIBRARY_PATH}/Release/${MONO_LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
	)
	set(MONO_LIB ${MONO_LIB} ws2_32.lib winmm.lib version.lib bcrypt.lib)
else()
	add_library(${MONO_LIB} STATIC IMPORTED)
	set_target_properties(${MONO_LIB} PROPERTIES
			IMPORTED_LOCATION "${MONO_LIBRARY_PATH}/${MONO_LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
	)
	add_library(${MONO_NATIVE_LIB} STATIC IMPORTED)
	set_target_properties(${MONO_NATIVE_LIB} PROPERTIES
			IMPORTED_LOCATION "${MONO_LIBRARY_PATH}/${MONO_NATIVE_LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
	)
	set(MONO_LIB -Wl,--whole-archive ${MONO_NATIVE_LIB} -Wl,--no-whole-archive ${MONO_LIB})
endif()

#
# C# Language Module for Plugify
#
file(GLOB_RECURSE CSHARPLM_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp")
set(CSHARPLM_PCH_FILE "src/pch.h")

add_library(${PROJECT_NAME} SHARED ${CSHARPLM_SOURCES})

set(CSHARPLM_LINK_LIBRARIES plugify::plugify plugify::plugify-function asmjit::asmjit glaze::glaze dyncall_s)

if(NOT COMPILER_SUPPORTS_FORMAT)
	set(CSHARPLM_LINK_LIBRARIES ${CSHARPLM_LINK_LIBRARIES} fmt::fmt-header-only)
endif()

include_directories(external/dyncall/)
include_directories(external/mono/include)

target_link_libraries(${PROJECT_NAME} PRIVATE ${MONO_LIB} ${CSHARPLM_LINK_LIBRARIES})
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CSHARPLM_PCH_FILE})

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wextra -Wconversion -Werror) # -Wshadow -Wpedantic
endif()

if(APPLE)
	target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/sym/exported_symbols.lds")
elseif(UNIX)
	target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/sym/version_script.lds")
endif()

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME} EXPORT_MACRO_NAME CSHARPLM_EXPORT EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/exports/module_export.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/exports)

target_compile_definitions(${PROJECT_NAME} PRIVATE CSHARPLM_PLATFORM="${CSHARPLM_PLATFORM}")