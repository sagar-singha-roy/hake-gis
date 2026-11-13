set(CPACK_GENERATOR)
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BundleConfig.cmake")

add_custom_target(bundle
                  COMMAND ${CMAKE_CPACK_COMMAND} "--config" "${CMAKE_BINARY_DIR}/BundleConfig.cmake" "--verbose"
                  COMMENT "Running CPACK. Please wait..."
                  DEPENDS qgis)

if(WIN32 AND NOT UNIX)
  set (CREATE_NSIS FALSE CACHE BOOL "Create an installer using NSIS")
endif()
set (CREATE_ZIP FALSE CACHE BOOL "Create a ZIP package")
set (CREATE_DEB FALSE CACHE BOOL "Create a DEB package")

# Do not warn about runtime libs when building using VS Express
if(NOT DEFINED CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
endif()

if(QGIS_INSTALL_SYS_LIBS)
  include(InstallRequiredSystemLibraries)
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${HAKE_PRODUCT_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "Hake Technologies")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${HAKE_PRODUCT_INSTALL_DIRECTORY}")
set(CPACK_PACKAGE_EXECUTABLES "hake-geodesk" "${HAKE_PRODUCT_DISPLAY_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_FILE_NAME "${HAKE_INSTALLER_FILE_NAME}")

if(CREATE_NSIS)
  # There is a bug in NSI that does not handle full unix paths properly. Make
  # sure there is at least one set of four (4) backslashes.
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\\\\\hake-geodesk.exe")
  set(CPACK_NSIS_PACKAGE_NAME "${HAKE_INSTALLER_NAME}")
  set(CPACK_NSIS_DISPLAY_NAME "${HAKE_INSTALLER_NAME}")
  set(CPACK_NSIS_HELP_LINK "https:\\\\\\\\haketech.com")
  set(CPACK_NSIS_URL_INFO_ABOUT "https:\\\\\\\\haketech.com")
  set(CPACK_NSIS_CONTACT "support@haketechnologies.com")
  # Do NOT append install\bin to the Windows system PATH.
  # That frequently hits Windows' PATH length limit ("environment variable is too large")
  # on developer machines. hake-geodesk.exe loads DLLs via hake-geodesk.env + AddDllDirectory.
  set(CPACK_NSIS_MODIFY_PATH OFF)
  list(APPEND CPACK_GENERATOR "NSIS")
endif()

if(CREATE_ZIP)
  list(APPEND CPACK_GENERATOR "ZIP")
endif()

if(CREATE_DEB)
  list(APPEND CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hake Technologies")
  set(CPACK_DEBIAN_PACKAGE_NAME "hake-geodesk")
  set(CPACK_DEBIAN_FILE_NAME "${HAKE_INSTALLER_FILE_NAME}.deb")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  # Declare Python runtime and plugin dependencies so apt automatically installs them
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "python3, python3-pyqt6, python3-pyqt6.sip, pyqt6-dev-tools, python3-packaging, python3-pyqt6.qsci, python3-pyqt6.qtsvg, python3-pyqt6.qtpositioning, python3-pyqt6.qtserialport, python3-requests, python3-yaml, python3-jinja2, python3-pyproj, python3-psycopg2, libsqlite3-mod-spatialite")
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND QGIS_MAC_BUNDLE)
  set(CREATE_DMG FALSE CACHE BOOL "Create a dmg bundle")
  set(PYMACDEPLOYQT_EXECUTABLE "${CMAKE_SOURCE_DIR}/platform/macos/pymacdeployqt.py")

  configure_file("${CMAKE_SOURCE_DIR}/platform/macos/Info.plist.in" "${CMAKE_BINARY_DIR}/platform//macos/Info.plist" @ONLY)
  install(FILES "${CMAKE_BINARY_DIR}/platform/macos/Info.plist" DESTINATION "${APP_CONTENTS_DIR}")

  set(CPACK_DMG_VOLUME_NAME "${HAKE_INSTALLER_NAME}")
  set(CPACK_DMG_FORMAT "UDBZ")
  list(APPEND CPACK_GENERATOR "External")
  message(STATUS "   + macdeployqt/DMG                      YES ")
  configure_file(${CMAKE_SOURCE_DIR}/platform/macos/CPackMacDeployQt.cmake.in "${CMAKE_BINARY_DIR}/CPackExternal.cmake" @ONLY)
  set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_BINARY_DIR}/CPackExternal.cmake")
  set(CPACK_EXTERNAL_ENABLE_STAGING ON)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/${QGIS_APP_NAME}.app")
endif()

include(CPack)
