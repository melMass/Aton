#==========
#
# Copyright (c) 2010, Dan Bethell, Johannes Saam.
# All rights reserved.
#
# For license information regarding redistribution and
# use, please refer to the COPYING file.
#
#==========

cmake_minimum_required( VERSION 2.6 )
project( renderconnect )

#=====
# General
set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/config/cmake )
find_package( Boost 1.40.0 COMPONENTS system REQUIRED )
find_package( Nuke REQUIRED )
find_package( Doxygen )

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${Boost_INCLUDE_DIRS}
  ${Nuke_INCLUDE_DIR}
  )

#=====
# Build the Nuke plugin
add_library( nuke_plugin 
  SHARED
  ${CMAKE_SOURCE_DIR}/src/nk_renderConnect.cpp 
  ${CMAKE_SOURCE_DIR}/src/Server.cpp
  ${CMAKE_SOURCE_DIR}/src/Client.cpp
  ${CMAKE_SOURCE_DIR}/src/Data.cpp
  )

set_target_properties( nuke_plugin
  PROPERTIES
  PREFIX ""
  OUTPUT_NAME "RenderConnect"
  COMPILE_FLAGS "-DUSE_GLEW ${Nuke_COMPILE_FLAGS}"
  LINK_FLAGS "${Nuke_LINK_FLAGS}"
  )

target_link_libraries( nuke_plugin 
  ${Boost_LIBRARIES}
  ${Nuke_LIBRARIES}
  )

#=====
# Build the Rman plugin
# try to find a rman lib (set by the RMAN envvar)
set( RMAN "3Delight" )
if ( $ENV{RMAN} MATCHES "" )
  set( RMAN $ENV{RMAN} )
endif ( $ENV{RMAN} MATCHES "" )
find_package( ${RMAN} )

if( PRMAN_FOUND )
    include_directories( ${${RMAN}_INCLUDE_DIR} )
    add_library( rman_plugin
      SHARED
      ${CMAKE_SOURCE_DIR}/src/d_rmanConnect.cpp
      ${CMAKE_SOURCE_DIR}/src/Client.cpp
      ${CMAKE_SOURCE_DIR}/src/Data.cpp
      )

    set_target_properties( rman_plugin
      PROPERTIES
      PREFIX ""
      OUTPUT_NAME "d_rmanConnect"
      COMPILE_FLAGS "${${RMAN}_COMPILE_FLAGS}"
    )

    # PRMan requires .so even on MacOSX
    if( RMAN MATCHES "PRMan" )
        set_target_properties( rman_plugin
                               PROPERTIES
                               SUFFIX ".so"
                               )
    endif( RMAN MATCHES "PRMan" )

    target_link_libraries( rman_plugin
      ${Boost_LIBRARIES}
      ${${RMAN}_LIBRARIES}
      )
endif( PRMAN_FOUND )

#=====
# Build the Arnold plugin
find_package( Arnold )

if( ARNOLD_FOUND )
    include_directories( ${Arnold_INCLUDE_DIR} )
    add_library( arnold_plugin
      SHARED
      ${CMAKE_SOURCE_DIR}/src/d_arnoldConnect.cpp
      ${CMAKE_SOURCE_DIR}/src/Client.cpp
      ${CMAKE_SOURCE_DIR}/src/Data.cpp
      )

    set_target_properties( arnold_plugin
      PROPERTIES
      PREFIX ""
      OUTPUT_NAME "d_arnoldConnect"
      COMPILE_FLAGS "${Arnold_COMPILE_FLAGS}"
    )

    target_link_libraries( arnold_plugin
      ${Boost_LIBRARIES}
      ${Arnold_ai_LIBRARY}
      )
endif( ARNOLD_FOUND )

#=====
# Build docs (after the nuke plugin is built)
IF( DOXYGEN_FOUND )
    ADD_CUSTOM_COMMAND( TARGET nuke_plugin
        POST_BUILD
        COMMAND ${DOXYGEN_EXECUTABLE} 
        ${CMAKE_SOURCE_DIR}/"config/docs/doxygen.cfg" 
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
ENDIF( DOXYGEN_FOUND )
