cmake_minimum_required(VERSION 3.5)
project(nasmw)

include(ExternalProject)

# Prevent this on UWP ARM build
if(NOT CMAKE_GENERATOR MATCHES ARM)
  externalproject_add(nasmw
                      SOURCE_DIR ${PROJECT_SOURCE_DIR}
                      BINARY_DIR ${PROJECT_SOURCE_DIR}
                      INSTALL_DIR ${PROJECT_SOURCE_DIR}
                      CONFIGURE_COMMAND ""
                      BUILD_COMMAND nmake /f Mkfiles/msvc.mak
                      INSTALL_COMMAND "")
else()
  externalproject_add(nasmw
                      SOURCE_DIR ${PROJECT_SOURCE_DIR}
                      BINARY_DIR ${PROJECT_SOURCE_DIR}
                      INSTALL_DIR ${PROJECT_SOURCE_DIR}
                      CONFIGURE_COMMAND ""
                      BUILD_COMMAND cmake -E echo "NASM not used on UWP ARM"
                      INSTALL_COMMAND "")
endif()

install(PROGRAMS nasm.exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                          RENAME nasmw.exe
                          OPTIONAL)