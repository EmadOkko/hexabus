# -*- mode: cmake; -*-
project(hxb-net-autoconfig)
cmake_minimum_required(VERSION "2.8" FATAL_ERROR)
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif(POLICY CMP0011)

if( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
   message(STATUS "Do not run cmake in the source directory")
   message(STATUS "create an extra binary directory")
   message(FATAL_ERROR "Exiting cmake here.")
endif( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

# have the full monty in makefiles
set(CMAKE_VERBOSE_MAKEFILE true)

SET(ENABLE_LOGGING 1)

# use ctest
ENABLE_TESTING()

set(V_MAJOR 0)
set(V_MINOR 1)
set(V_PATCH 0)

# add a path where some libraries might be stored
set(CMAKE_ADDITIONAL_PATH "$ENV{CMAKE_ADDITIONAL_PATH}" CACHE PATH "Path where many locally installed libraries can be found")

###################################################################################################
# No user settings below
###################################################################################################

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/modules ${CMAKE_SOURCE_DIR}/../cmake_modules ${CMAKE_MODULE_PATH})

# enable unit testing
include(CTest)
enable_testing()

# add some files to the installation target
#INSTALL(FILES README.md LICENSE.txt DESTINATION share/doc/hexaswitch)
INSTALL(PROGRAMS ipaddress.py hxb-net-autoconf.py DESTINATION lib/hxb-net-autoconf)
INSTALL(PROGRAMS hxb-net-autoconf DESTINATION bin)

# CPACK packaging
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "DEB" "STGZ" "TGZ" "TBZ2")
set(CPACK_PACKAGE_VENDOR "Fraunhofer ITWM")
set(CPACK_PACKAGE_CONTACT "Fraunhofer ITWM <support@mysmartgrid.de>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hexabus RasPi network autoconfiguration")
#set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
#set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_VERSION_MAJOR ${V_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${V_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${V_PATCH})

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "all")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "python3 (>= 3.2.3), radvd, mrd6")

set(CPACK_SECTION "net")
include(CPackTools)

### print some output for the user
message("")
message("***** Configuration *****")
message("  prefix: ${CMAKE_INSTALL_PREFIX}")
message("  extended logging: ${ENABLE_LOGGING}")
