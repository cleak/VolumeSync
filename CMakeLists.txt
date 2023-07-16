cmake_minimum_required(VERSION 3.8)
project(VolumeSync)

set(CMAKE_CXX_STANDARD 17)

set(SOURCES main.cpp Audio.cpp Audio.h resource.h Config.cpp Config.h)
add_executable(VolumeSync WIN32 ${SOURCES} resources.rc)

target_link_libraries(VolumeSync ole32.lib uuid.lib user32 shell32)

#-------------------------------------------------------------------------------
# Installer

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "VolumeSync")
set(CPACK_PACKAGE_VENDOR "Caleb Leaky")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VolumeSync - A simple service to sync volume across multiple audio devices")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VolumeSync")

set(CPACK_GENERATOR "NSIS")

set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY} VolumeSync")

set(CPACK_NSIS_HELP_LINK "https://github.com/cleak/VolumeSync")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/cleak/VolumeSync")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# set(CPACK_NSIS_CONTACT "contact@mycompany.com")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecuteNSIS '${CMAKE_CURRENT_SOURCE_DIR}/UninstallScript.nsi'")

include(CPack)
