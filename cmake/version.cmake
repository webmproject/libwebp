# CMake script to generate the CMake version file from an executable
# that outputs the WebP version.

execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/version_checker
  OUTPUT_VARIABLE WebP_VERSION
)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/WebPConfigVersion.cmake"
  VERSION ${WebP_VERSION}
  COMPATIBILITY AnyNewerVersion
)
