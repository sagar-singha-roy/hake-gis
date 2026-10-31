# Writes qgsversion.h and qgsversion.inc with the Hake Geospatial code revision,
# i.e. the Git commit this build was produced from.
#
# Run by CREATE_QGSVERSION() at configure time and by the "version" target on
# every build. Inputs come from the file named by HAKE_VERSION_PARAMS (written
# at configure time) so no value has to survive shell quoting.
#
# Resolution order:
#   1. HAKE_EXPLICIT_REVISION (from -DHAKE_GIT_REVISION, validated at configure)
#   2. ENABLE_LOCAL_BUILD_SHORTCUTS -> "dev"
#   3. git rev-parse HEAD, only if HAKE_SOURCE_DIR is the top of the work tree
#   4. HAKE_FALLBACK_SHA (-DSHA, e.g. debian/rules)
#   5. "unknown"
# Outputs are only rewritten when their content changes.

if(NOT HAKE_VERSION_PARAMS)
  message(FATAL_ERROR "HakeWriteQgsVersion.cmake: HAKE_VERSION_PARAMS is not set")
endif()
include("${HAKE_VERSION_PARAMS}")

set(_revision "")
set(_short "")
set(_commit_url "")

if(HAKE_EXPLICIT_REVISION)
  set(_revision "${HAKE_EXPLICIT_REVISION}")
elseif(HAKE_LOCAL_BUILD_SHORTCUTS)
  set(_short "dev")
elseif(HAKE_GIT_EXECUTABLE)
  execute_process(
    COMMAND "${HAKE_GIT_EXECUTABLE}" rev-parse --show-toplevel
    WORKING_DIRECTORY "${HAKE_SOURCE_DIR}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _toplevel
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(_rc EQUAL 0)
    get_filename_component(_toplevel "${_toplevel}" REALPATH)
    get_filename_component(_source "${HAKE_SOURCE_DIR}" REALPATH)
    if(CMAKE_HOST_WIN32)
      string(TOLOWER "${_toplevel}" _toplevel)
      string(TOLOWER "${_source}" _source)
    endif()
    if(_toplevel STREQUAL _source)
      execute_process(
        COMMAND "${HAKE_GIT_EXECUTABLE}" rev-parse --verify HEAD
        WORKING_DIRECTORY "${HAKE_SOURCE_DIR}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      string(TOLOWER "${_head}" _head)
      string(LENGTH "${_head}" _len)
      if(_rc EQUAL 0 AND _head MATCHES "^[0-9a-f]+$" AND (_len EQUAL 40 OR _len EQUAL 64))
        set(_revision "${_head}")
      endif()
    endif()
  endif()
endif()

if(_revision)
  string(SUBSTRING "${_revision}" 0 8 _short)
  set(_commit_url "${HAKE_REPOSITORY_URL}/commit/${_revision}")
elseif(NOT _short)
  if(HAKE_FALLBACK_SHA)
    set(_short "${HAKE_FALLBACK_SHA}")
  else()
    set(_short "unknown")
  endif()
  set(_revision "${_short}")
endif()

set(_header "#define QGSVERSION \"${_short}\"
#define HAKE_GIT_REVISION \"${_revision}\"
#define HAKE_GIT_REVISION_SHORT \"${_short}\"
#define HAKE_GIT_COMMIT_URL \"${_commit_url}\"
#define QGS_GIT_REMOTE_URL \"${HAKE_REPOSITORY_URL}\"
")
set(_inc "PROJECT_NUMBER = \"${HAKE_COMPLETE_VERSION}-${HAKE_RELEASE_NAME} (${_short})\"\n")

function(_hake_write_if_different path content)
  if(EXISTS "${path}")
    file(READ "${path}" _old)
    if(_old STREQUAL content)
      return()
    endif()
  endif()
  file(WRITE "${path}" "${content}")
endfunction()

_hake_write_if_different("${HAKE_BINARY_DIR}/qgsversion.h" "${_header}")
_hake_write_if_different("${HAKE_BINARY_DIR}/qgsversion.inc" "${_inc}")
