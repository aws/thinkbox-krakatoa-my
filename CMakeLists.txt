# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

project( KrakatoaMY )

find_package( thinkboxcmlibrary REQUIRED )
include( PrecompiledHeader)
include( ThinkboxCMLibrary)

set( SUPPORTED_MAYA_VERSIONS 2022 2023 )

option( MAYA_VERSION "The version of Maya SDK to build the library against." 2022 )

if( NOT MAYA_VERSION IN_LIST SUPPORTED_MAYA_VERSIONS )
  message( FATAL_ERROR "ERROR: Cannot build for unsupported Maya version ${MAYA_VERSION}" )
endif()

add_library( krakatoamy SHARED )

set_target_properties( krakatoamy PROPERTIES OUTPUT_NAME "MayaKrakatoa" )
set_target_properties( krakatoamy PROPERTIES PREFIX "" )

target_include_directories( krakatoamy PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include> )
target_include_directories( krakatoamy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

if( WIN32 )
	set_target_properties( krakatoamy PROPERTIES SUFFIX ".mll" )
elseif( APPLE )
	set_target_properties( krakatoamy PROPERTIES SUFFIX ".bundle" )
elseif( UNIX )
	set_target_properties( krakatoamy PROPERTIES SUFFIX ".so" )
endif()

file( GLOB_RECURSE H_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "include/*.h" "include/*.hpp" )	
file( GLOB_RECURSE CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp" )

target_sources( krakatoamy PRIVATE
	"KrakatoaVersion.h"
	"stdafx.h"
	"stdafx.cpp"
	${H_FILES}
	${CXX_FILES}
)

# The Conan version of Boost was built with this, and it changes the library names.
# As a result, we need to set this to tell Boost to look for the right libraries to
# link against.
target_compile_definitions( krakatoamy PUBLIC BOOST_AUTO_LINK_SYSTEM )
target_compile_definitions( krakatoamy PUBLIC KSR_STATIC )

find_package( mayasdk REQUIRED )
find_package( thinkboxlibrary REQUIRED )
find_package( thinkboxmylibrary REQUIRED )
find_package( krakatoa REQUIRED )
find_package( krakatoasr REQUIRED )
find_package( magma REQUIRED )
find_package( magmamy REQUIRED )
find_package( nodeview REQUIRED )
find_package( Boost REQUIRED )
find_package( OpenEXR REQUIRED )
find_package( ZLIB REQUIRED )
find_package( TBB REQUIRED )
find_package( xxHash REQUIRED )

target_include_directories( krakatoamy PUBLIC ${mayasdk_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${thinkboxlibrary_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${thinkboxmylibrary_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${krakatoa_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${krakatoasr_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${magma_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${magmamy_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${nodeview_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${Boost_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${OpenEXR_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${ZLIB_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${TBB_INCLUDE_DIRS} )
target_include_directories( krakatoamy PUBLIC ${xxHash_INCLUDE_DIRS} )

target_link_libraries( krakatoamy PUBLIC mayasdk::mayasdk )
target_link_libraries( krakatoamy PUBLIC thinkboxlibrary::thinkboxlibrary )
target_link_libraries( krakatoamy PUBLIC thinkboxmylibrary::thinkboxmylibrary )
target_link_libraries( krakatoamy PUBLIC krakatoa::krakatoa )
target_link_libraries( krakatoamy PUBLIC krakatoasr::krakatoasr )
target_link_libraries( krakatoamy PUBLIC magma::magma )
target_link_libraries( krakatoamy PUBLIC magmamy::magmamy )
target_link_libraries( krakatoamy PUBLIC nodeview::nodeview )
target_link_libraries( krakatoamy PUBLIC Boost::Boost )
target_link_libraries( krakatoamy PUBLIC OpenEXR::OpenEXR )
target_link_libraries( krakatoamy PUBLIC ZLIB::ZLIB )
target_link_libraries( krakatoamy PUBLIC TBB::tbb )
target_link_libraries( krakatoamy PUBLIC xxHash::xxHash )

find_package( OpenGL REQUIRED )
include_directories( ${OPENGL_INCLUDE_DIRS} )
target_link_libraries( krakatoamy PUBLIC ${OPENGL_LIBRARIES} )

frantic_common_platform_setup( krakatoamy )
frantic_default_source_groups( krakatoamy HEADERDIR include SOURCEDIR src )

if( APPLE )
	set_property( TARGET krakatoamy APPEND_STRING PROPERTY LINK_FLAGS "-Wl,-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/MayaKrakatoa.exp" )
	# These are required when including certain Maya SDK headers.
	target_link_libraries( krakatoamy PUBLIC "-framework CoreServices")
	target_link_libraries( krakatoamy PUBLIC "-framework CoreFoundation")
	target_link_libraries( krakatoamy PUBLIC "-framework IOKit")
elseif( UNIX )
	set_property( TARGET krakatoamy APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--no-undefined -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/MayaKrakatoa.map -ldl" )
endif()

frantic_common_platform_setup( krakatoamy )
frantic_default_source_groups( krakatoamy HEADERDIR include SOURCEDIR src )

# Disable optimization for the RelWithDebInfo configuration on Windows.
# This allows breakpoints to be hit reliably when debugging in Visual Studio.
if( WIN32 )
	target_compile_options( krakatoamy PRIVATE "$<$<CONFIG:Release>:/O2>$<$<CONFIG:RelWithDebInfo>:/Od>" )
endif()

install( TARGETS krakatoamy
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
)
