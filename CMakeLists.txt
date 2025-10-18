cmake_minimum_required(VERSION 3.16)

if(APPLE)
  if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
  endif()
endif()

# ---- Derive version string ----
set(_fireminipro_version "0.0.0")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --always --match "v[0-9]*"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE _git_describe
      RESULT_VARIABLE _git_result
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_git_result EQUAL 0 AND NOT _git_describe STREQUAL "")
      set(_fireminipro_version "${_git_describe}")
    endif()
  endif()
endif()

if(_fireminipro_version STREQUAL "0.0.0")
  set(_version_file "${CMAKE_SOURCE_DIR}/cmake/version.txt")
  if(EXISTS "${_version_file}")
    file(READ "${_version_file}" _archived_version)
    string(STRIP "${_archived_version}" _archived_version)
    if(NOT _archived_version MATCHES "\\$Format")
      set(_fireminipro_version "${_archived_version}")
    endif()
  endif()
endif()

string(REGEX REPLACE "^v" "" FIREMINIPRO_VERSION_STRING "${_fireminipro_version}")
string(REGEX REPLACE "^v" "" _version_clean "${_fireminipro_version}")
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _numeric_version "${_version_clean}")
if(_numeric_version STREQUAL "")
  set(_numeric_version "0.0.0")
endif()

project(fireminipro VERSION ${_numeric_version} LANGUAGES CXX)

# --- Qt setup ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets DBus)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/HexView.cpp
    src/ProcessHandling.cpp
    src/LoadPreviewBar.cpp
    src/SegmentView.cpp
    src/SegmentTableView.cpp
)
set(HEADERS
    src/MainWindow.h
    src/HexView.h
    src/ProcessHandling.h
    src/LoadPreviewBar.h
    src/SegmentView.h
    src/SegmentTableView.h
)

# Use AUTORCC by listing the qrc directly here.
qt_add_executable(${PROJECT_NAME} MACOSX_BUNDLE
    ${SOURCES} ${HEADERS}
    resources/icons.qrc
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::DBus)
# Expose PROJECT_VERSION to C++ as FIREMINIPRO_VERSION
target_compile_definitions(${PROJECT_NAME}
    PRIVATE FIREMINIPRO_VERSION="${FIREMINIPRO_VERSION_STRING}")


# ------------ macOS app icon (.icns) generation ------------
# We’ll try to build appicon.icns from resources/appicon.png automatically.
# If the tools aren't found, we fall back to using a prebuilt appicon.icns
# if you place it at resources/appicon.icns.

if(APPLE)
  set(APPICON_PNG  "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.png")
  set(APPICON_ICNS "${CMAKE_CURRENT_BINARY_DIR}/appicon.icns")
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/appicon.icns"
    "${CMAKE_CURRENT_BINARY_DIR}/appicon.iconset"
    )
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_CURRENT_BINARY_DIR}/appicon.icns"
    "${CMAKE_CURRENT_BINARY_DIR}/appicon.iconset"
  )

  # If a prebuilt icns is provided in the source tree, prefer that.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.icns")
    set(APPICON_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.icns")
  else()
    # Try to generate .icns from the PNG using sips + iconutil.
    find_program(SIPS_EXEC sips)
    find_program(ICONUTIL_EXEC iconutil)

    if(SIPS_EXEC AND ICONUTIL_EXEC AND EXISTS "${APPICON_PNG}")
      set(ICONSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/appicon.iconset")

      add_custom_command(
        OUTPUT "${APPICON_ICNS}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${ICONSET_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ICONSET_DIR}"

        # Create all the standard sizes (1x and 2x) from the base PNG.
        # If your base PNG is large (>=1024), quality will be fine.
        COMMAND "${SIPS_EXEC}" -z 16   16   "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_16x16.png"
        COMMAND "${SIPS_EXEC}" -z 32   32   "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_16x16@2x.png"
        COMMAND "${SIPS_EXEC}" -z 32   32   "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_32x32.png"
        COMMAND "${SIPS_EXEC}" -z 64   64   "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_32x32@2x.png"
        COMMAND "${SIPS_EXEC}" -z 128  128  "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_128x128.png"
        COMMAND "${SIPS_EXEC}" -z 256  256  "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_128x128@2x.png"
        COMMAND "${SIPS_EXEC}" -z 256  256  "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_256x256.png"
        COMMAND "${SIPS_EXEC}" -z 512  512  "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_256x256@2x.png"
        COMMAND "${SIPS_EXEC}" -z 512  512  "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_512x512.png"
        # 1024x1024 for @2x of 512
        COMMAND "${SIPS_EXEC}" -z 1024 1024 "${APPICON_PNG}" --out "${ICONSET_DIR}/icon_512x512@2x.png"

        COMMAND "${ICONUTIL_EXEC}" -c icns "${ICONSET_DIR}" -o "${APPICON_ICNS}"
        DEPENDS "${APPICON_PNG}"
        VERBATIM
      )

      add_custom_target(AppIcon ALL DEPENDS "${APPICON_ICNS}")
      add_dependencies(${PROJECT_NAME} AppIcon)
    else()
      message(WARNING
        "macOS icon generation skipped: 'sips' or 'iconutil' not found, "
        "or resources/appicon.png missing. "
        "Provide resources/appicon.icns to use a prebuilt icon.")
      # If we can't build one, we leave APPICON_ICNS unset unless a prebuilt exists.
      unset(APPICON_ICNS)
    endif()
  endif()

  # If we have an icns (generated or prebuilt), add it to the bundle.
  if(APPICON_ICNS)
    get_filename_component(_icns_name "${APPICON_ICNS}" NAME)
    get_filename_component(_icns_stem "${APPICON_ICNS}" NAME_WE)
    # Put the icns into the app bundle's Resources folder
    set_source_files_properties("${APPICON_ICNS}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE "${APPICON_ICNS}")

    # Tell macOS which icon file to use (filename only)
    set_target_properties(${PROJECT_NAME} PROPERTIES
      MACOSX_BUNDLE_ICON_FILE "${_icns_stem}"
    )

  endif()
endif()

# ------------- (optional) nicer bundle metadata -------------
# set_target_properties(${PROJECT_NAME} PROPERTIES
#   MACOSX_BUNDLE_BUNDLE_NAME "fireminipro"
#   MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.fireminipro"
#   MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
#   MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
# )
if(APPLE)
  if(NOT DEFINED _icns_name OR _icns_name STREQUAL "")
    set(_icns_name "appicon.icns")
  endif()
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${APPICON_ICNS}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/${_icns_name}"
    COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/${_icns_name}"
    COMMAND ${CMAKE_COMMAND} -E touch "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Info.plist"
    COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
    COMMAND /bin/sh -c "/usr/bin/osascript -e 'tell application \\\"Finder\\\" to update POSIX file \\\"$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>\\\"' || true"
    COMMENT "Touching bundle assets to refresh Finder icon"
  )
endif()
