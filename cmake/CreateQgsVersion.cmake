# Creates version files
#  qgsversion.h that defines QGSVERSION (short code revision) and
#    HAKE_GIT_REVISION / HAKE_GIT_REVISION_SHORT / HAKE_GIT_COMMIT_URL
#  qgsversion.inc for doxygen
#
# The code revision is the Git commit the build was produced from. CI passes it
# explicitly with -DHAKE_GIT_REVISION=<full sha>; otherwise the source tree's
# HEAD is used, see cmake/HakeWriteQgsVersion.cmake.

function(CREATE_QGSVERSION)
  # Consume the explicit revision and drop it from the cache so a later
  # reconfigure without -DHAKE_GIT_REVISION cannot reuse a stale commit.
  string(STRIP "${HAKE_GIT_REVISION}" _explicit)
  string(TOLOWER "${_explicit}" _explicit)
  unset(HAKE_GIT_REVISION CACHE)
  if(_explicit)
    string(LENGTH "${_explicit}" _len)
    if(NOT _explicit MATCHES "^[0-9a-f]+$" OR NOT (_len EQUAL 40 OR _len EQUAL 64))
      message(FATAL_ERROR "HAKE_GIT_REVISION must be a full hexadecimal Git commit SHA, got '${_explicit}'")
    endif()
  endif()

  find_package(Git QUIET)
  string(REGEX REPLACE "/+$" "" _repo_url "${HAKE_GIT_REPOSITORY_URL}")

  set(_params "${CMAKE_BINARY_DIR}/hake_version_params.cmake")
  file(WRITE "${_params}"
    "set(HAKE_EXPLICIT_REVISION [==[${_explicit}]==])\n"
    "set(HAKE_LOCAL_BUILD_SHORTCUTS [==[${ENABLE_LOCAL_BUILD_SHORTCUTS}]==])\n"
    "set(HAKE_GIT_EXECUTABLE [==[${GIT_EXECUTABLE}]==])\n"
    "set(HAKE_SOURCE_DIR [==[${CMAKE_SOURCE_DIR}]==])\n"
    "set(HAKE_BINARY_DIR [==[${CMAKE_BINARY_DIR}]==])\n"
    "set(HAKE_FALLBACK_SHA [==[${SHA}]==])\n"
    "set(HAKE_REPOSITORY_URL [==[${_repo_url}]==])\n"
    "set(HAKE_COMPLETE_VERSION [==[${COMPLETE_VERSION}]==])\n"
    "set(HAKE_RELEASE_NAME [==[${RELEASE_NAME}]==])\n"
  )

  set(_script "${CMAKE_SOURCE_DIR}/cmake/HakeWriteQgsVersion.cmake")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DHAKE_VERSION_PARAMS=${_params}" -P "${_script}"
    RESULT_VARIABLE _rc
  )
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Failed to generate qgsversion.h")
  endif()
  file(STRINGS "${CMAKE_BINARY_DIR}/qgsversion.h" _rev_line REGEX "HAKE_GIT_REVISION ")
  message(STATUS "Hake Geospatial code revision: ${_rev_line}")

  # Re-evaluated on every build; outputs only change when the revision does.
  add_custom_target(version ALL
    COMMAND "${CMAKE_COMMAND}" "-DHAKE_VERSION_PARAMS=${_params}" -P "${_script}"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/qgsversion.h" "${CMAKE_BINARY_DIR}/qgsversion.inc"
    COMMENT "Updating Hake Geospatial code revision"
    VERBATIM
  )
endfunction()

# Add the win32 resource for the QGIS icon
# Only runs for WIN32 when called
function(win32_icon SRC_LIST)
  if(NOT WIN32)
    return()
  endif()

  set(OUT "${PROJECT_SOURCE_DIR}/platform/windows/rc/icon.rc")
  list(APPEND ${SRC_LIST} "${OUT}")

  set(${SRC_LIST} "${${SRC_LIST}}" PARENT_SCOPE)
endfunction()

# Function to create win32 VERSIONINFO and resource blocks
# Only runs for WIN32 when called
function(win32_version_info desc filename SRC_LIST)
  if(NOT WIN32)
    return()
  endif()

  set(WIN32_VI_DESC ${desc})
  set(WIN32_VI_FILENAME ${filename})
  set(WIN32_VI_COMPANY "Hake Technologies")
  set(WIN32_VI_PRODUCT "${HAKE_PRODUCT_DISPLAY_NAME}")
  set(OUT "${CMAKE_CURRENT_BINARY_DIR}/${filename}_version.rc")
  configure_file("${PROJECT_SOURCE_DIR}/platform/windows/rc/version.rc.in" "${OUT}" @ONLY)
  list(APPEND ${SRC_LIST} "${OUT}")

  set(${SRC_LIST} "${${SRC_LIST}}" PARENT_SCOPE)
endfunction()

