set(NUGET_SOURCE "https://nuget.pkg.github.com/Subhankar-hub/index.json" CACHE STRING "Nuget push source for vcpkg binary cache")
set(NUGET_USERNAME "Subhankar-hub" CACHE STRING "Nuget user for push source")
set(NUGET_READ_SOURCE "https://nuget.pkg.github.com/Subhankar-hub/index.json" CACHE STRING "Read-only Nuget source for vcpkg binary cache (upstream QGIS packages)")
set(NUGET_READ_USERNAME "Subhankar-hub" CACHE STRING "Nuget user for read-only source")

# Setup features (dependencies) based on cmake configuration
if(WITH_3D)
  list(APPEND VCPKG_MANIFEST_FEATURES "3d")
endif()
if(WITH_AUTH)
  list(APPEND VCPKG_MANIFEST_FEATURES "auth")
endif()
if(WITH_BINDINGS)
  list(APPEND VCPKG_MANIFEST_FEATURES "bindings")
endif()
if(WITH_EXIV2)
  list(APPEND VCPKG_MANIFEST_FEATURES "exiv2")
endif()
if(WITH_GUI)
  list(APPEND VCPKG_MANIFEST_FEATURES "gui")
endif()
if(WITH_ORACLE)
  list(APPEND VCPKG_MANIFEST_FEATURES "oracle")
endif()
if(WITH_PDAL)
  list(APPEND VCPKG_MANIFEST_FEATURES "pdal")
endif()
if(WITH_QTPOSITIONING)
  list(APPEND VCPKG_MANIFEST_FEATURES "qtpositioning")
endif()
if(WITH_SFCGAL)
  list(APPEND VCPKG_MANIFEST_FEATURES "sfcgal")
endif()
if(WITH_PROJ_DATA)
  list(APPEND VCPKG_MANIFEST_FEATURES "proj-data")
endif()
if(WITH_TRACY)
  list(APPEND VCPKG_MANIFEST_FEATURES "tracy")
endif()
# We cannot detect the EMSCRIPTEN variable yet, as we didn't load the toolchain file at this point.
# So we use the target triplet to determine if we are building for wasm.
if(VCPKG_TARGET_TRIPLET MATCHES "^wasm32-")
  set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON)
endif()

# Binarycache can only be used on Windows or if mono is available.
find_program(_VCPKG_MONO mono)
if(NOT "${NUGET_TOKEN}" STREQUAL "" AND (CMAKE_HOST_WIN32 OR EXISTS "${_VCPKG_MONO}"))
  if(CMAKE_HOST_WIN32)
    set(_VCPKG_EXECUTABLE "$ENV{VCPKG_ROOT}/vcpkg.exe")
  else()
    set(_VCPKG_EXECUTABLE "$ENV{VCPKG_ROOT}/vcpkg")
  endif()

  execute_process(
    COMMAND ${_VCPKG_EXECUTABLE} fetch nuget
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE _FETCH_NUGET_OUTPUT)

  STRING(REGEX REPLACE "\n" ";" _FETCH_NUGET_OUTPUT "${_FETCH_NUGET_OUTPUT}")
  list(GET _FETCH_NUGET_OUTPUT -1 _NUGET_PATH)

  if(CMAKE_HOST_WIN32)
    set(_NUGET_EXE ${_NUGET_PATH})
  else()
    set(_NUGET_EXE ${_VCPKG_MONO} ${_NUGET_PATH})
  endif()

  set(_CONFIG_PATH "${CMAKE_BINARY_DIR}/github-NuGet.Config")

  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/NuGet.Config.in"
    "${_CONFIG_PATH}"
    @ONLY)
  foreach(_nuget_feed IN ITEMS "${NUGET_READ_SOURCE}" "${NUGET_SOURCE}")
    if(_nuget_feed STREQUAL "")
      continue()
    endif()
    execute_process(
      COMMAND ${_NUGET_EXE} setapikey "${NUGET_TOKEN}" -src "${_nuget_feed}" -configfile ${_CONFIG_PATH}
      OUTPUT_VARIABLE _OUTPUT
      ERROR_VARIABLE _ERROR
      RESULT_VARIABLE _RESULT)
    if(_RESULT EQUAL 0)
      message(STATUS "Setup nuget api key for ${_nuget_feed} - done")
    else()
      message(STATUS "Setup nuget api key for ${_nuget_feed} - failed")
      message(STATUS "Output:")
      message(STATUS ${_OUTPUT})
      message(STATUS "Error:")
      message(STATUS ${_ERROR})
    endif()
  endforeach()

  file(TO_NATIVE_PATH "${_CONFIG_PATH}" _CONFIG_PATH_NATIVE)
  if("$ENV{GITHUB_EVENT_NAME}" STREQUAL "pull_request")
    set(ENV{VCPKG_BINARY_SOURCES} "$ENV{VCPKG_BINARY_SOURCES};nugetconfig,${_CONFIG_PATH_NATIVE},read")
  else()
    set(ENV{VCPKG_BINARY_SOURCES} "$ENV{VCPKG_BINARY_SOURCES};nugetconfig,${_CONFIG_PATH_NATIVE},readwrite")
  endif()
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_MANIFEST_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
# Copies DLLs built by vcpkg when an install() command is run.
# Only works on Windows and even there not reliably ...
# set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON CACHE BOOL "Copy dependency DLLs on install")
