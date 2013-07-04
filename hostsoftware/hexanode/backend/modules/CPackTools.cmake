# -*- mode: cmake; -*-

set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

# set system architecture
if( ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
elseif( ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
else()
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_SYSTEM_NAME ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE})
# set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "")

message(STATUS "CPACK_DEBIAN_PACKAGE_ARCHITECTURE: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CPACK_SYSTEM_NAME: ${CPACK_SYSTEM_NAME}")

set(CPACK_SET_DESTDIR On)

include(CPack)
