cmake_minimum_required(VERSION 3.5)
project(nasmw)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
