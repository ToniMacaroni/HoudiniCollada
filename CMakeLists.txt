cmake_minimum_required( VERSION 3.10 )

if ( NOT CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE Release )
endif ()

project( HoudiniCollada VERSION 1.0.0 DESCRIPTION "Collada Exporter for Houdini" )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED True )

list( APPEND CMAKE_PREFIX_PATH "C:/Program Files/Side Effects Software/Houdini 19.5.805/toolkit/cmake" )
find_package( Houdini REQUIRED )

set( CMAKE_CXX_FLAGS "-Wall" )
set( CMAKE_CXX_FLAGS_RELEASE "-O3" )

if (MSVC)
    SET( CMAKE_CXX_FLAGS "/EHsc" )
    SET( CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1 )
else ()
    set( CMAKE_CXX_FLAGS_DEBUG "-g" )
endif ()

set( exe_name HoudiniCollada )
set( lib_name HoudiniColladaLib )

include_directories( include )
include_directories( Thirdparty )

add_library( ${lib_name} SHARED
        src/MainLib.cpp
        include/MainLib.h
        Thirdparty/pugixml/pugixml.cpp
        )

target_link_libraries( ${lib_name} Houdini )
houdini_configure_target( ${lib_name} )