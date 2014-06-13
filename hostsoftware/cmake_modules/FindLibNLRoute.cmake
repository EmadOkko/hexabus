# - Try to find libnl-route-3
#
# Once done this will define
#
#  LIBNLROUTE_FOUND
#  LIBNLROUTE_INCLUDE_DIR
#  LIBNLROUTE_LIBRARIES
#  LIBNLROUTE_DEFINITIONS

IF (LIBNLROUTE_INCLUDE_DIR AND LIBNLROUTE_LIBRARY)
	SET(libnlroute_FIND_QUIETLY TRUE)
ENDIF (LIBNLROUTE_INCLUDE_DIR AND LIBNLROUTE_LIBRARY)

FIND_PACKAGE(PkgConfig QUIET)
PKG_CHECK_MODULES(PC_LIBNLROUTE QUIET libnl-route-3.0)
SET(LIBNLROUTE_DEFINITIONS ${PC_LIBNLROUTE_CFLAGS_OTHER})
SET(LIBNLROUTE_VERSION_STRING ${PC_LIBNLROUTE_VERSION})

FIND_PATH(LIBNLROUTE_INCLUDE_DIR netlink/route/route.h
	HINTS ${PC_LIBNLROUTE_INCLUDEDIR} ${PC_LIBNLROUTE_INCLUDE_DIRS})

FIND_LIBRARY(LIBNLROUTE_LIBRARY NAMES nl-route-3
	HINTS ${PC_LIBNLROUTE_LIBDIR} ${PC_LIBNLROUTE_LIBRARY_DIRS})

MARK_AS_ADVANCED(LIBNLROUTE_INCLUDE_DIR LIBNLROUTE_LIBRARY)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(libnlroute
	REQUIRED_VARS LIBNLROUTE_LIBRARY LIBNLROUTE_INCLUDE_DIR
	VERSION_VAR LIBNLROUTE_VERSION_STRING)

IF(LIBNLROUTE_FOUND)
	SET(LIBNLROUTE_LIBRARIES    ${LIBNLROUTE_LIBRARY})
	SET(LIBNLROUTE_INCLUDE_DIRS ${LIBNLROUTE_INCLUDE_DIR})
ENDIF()
