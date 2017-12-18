project(visualization.milkdrop2)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_program(CMAKE_ASM_NASM_COMPILER nasm
  PATHS ${ADDON_DEPENDS_PATH}/bin)

find_package(Kodi REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/vis_milk2
                    ${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways (becomes done in future)
                    )

set(MD2_SOURCES src/Milkdrop2XBMC.cpp)

add_subdirectory(lib/vis_milk2)

set(DEPLIBS vis_milk2)
if(CORE_SYSTEM_NAME STREQUAL windowsstore)
  add_definitions(-D_WIN32_WINNT=0x0A00 -D_WINDOWS -D_CRT_NON_CONFORMING_SWPRINTFS)
endif()

build_addon(visualization.milkdrop2 MD2 DEPLIBS)

add_executable(Milkdrop2StandAlone WIN32 EXCLUDE_FROM_ALL src/StandAlone.cpp)
target_link_libraries(Milkdrop2StandAlone ${DEPLIBS})

include(CPack)
