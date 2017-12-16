project(vis_milk2)

enable_language(C)
enable_language(CXX)
enable_language(ASM_NASM)

cmake_minimum_required(VERSION 2.6)

set( SHADER_FILES dx11/DefaultVertexShader.hlsl
                  dx11/DiffusePixelShader.hlsl
                  dx11/ColorPixelShader.hlsl
                  dx11/TexturePixelShader.hlsl
                  dx11/Texture2PixelShader.hlsl )
set( SHADER_INCLUDES )

foreach( SHADER_FILE ${SHADER_FILES} )
  get_filename_component( FileName ${SHADER_FILE} NAME_WE )
  if(${FileName} MATCHES "(.+Vertex.+)")
    set(ShaderType vs)
  elseif(${FileName} MATCHES "(.+Pixel.+)")
    set(ShaderType ps)
  else()
    message(FATAL_ERROR "Unknown shader type.")
  endif()
  set( IncFile ${FileName}.inc )
  list( APPEND SHADER_INCLUDES ${IncFile} )

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${IncFile}
    COMMAND fxc /nologo /T${ShaderType}_4_0_level_9_1 /Vn ${FileName}Code /Fh ${IncFile} ${SHADER_FILE}
    MAIN_DEPENDENCY ${SHADER_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)
endforeach(SHADER_FILE)

set(SOURCES dx11/CommonStates.cpp
            dx11/ConstantTable.cpp
            dx11/DDSTextureLoader.cpp
            dx11/WICTextureLoader.cpp
            dx11/DX11Context.cpp
            ns-eel2/eel_atomic.h
            ns-eel2/eel_eval.h
            ns-eel2/eel_fft.h
            ns-eel2/eel_files.h
            ns-eel2/eel_lice.h
            ns-eel2/eel_mdct.h
            ns-eel2/eel_misc.h
            ns-eel2/eel_net.h
            ns-eel2/eel_strings.h
            ns-eel2/eelscript.h
            ns-eel2/ns-eel-addfuncs.h
            ns-eel2/ns-eel-func-ref.h
            ns-eel2/ns-eel-int.h
            ns-eel2/ns-eel.h
            ns-eel2/nseel-caltab.c
            ns-eel2/nseel-cfunc.c
            ns-eel2/nseel-compiler.c
            ns-eel2/nseel-eval.c
            ns-eel2/nseel-lextab.c
            ns-eel2/nseel-ram.c
            ns-eel2/nseel-yylex.c
            ns-eel2/wdlcstring.h
            ns-eel2/wdlstring.h
            vis_milk2/dxcontext.cpp
            vis_milk2/fft.cpp
            vis_milk2/milkdropfs.cpp
            vis_milk2/plugin.cpp
            vis_milk2/pluginshell.cpp
            vis_milk2/state.cpp
            vis_milk2/support.cpp
            vis_milk2/texmgr.cpp
            vis_milk2/utility.cpp)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(SOURCES ${SOURCES} 
    ns-eel2/asm-nseel-x64.asm
    ns-eel2/glue_x86_64.h
  )
else()
  set(SOURCES ${SOURCES} 
    ns-eel2/asm-nseel-x86-msvc.c
    ns-eel2/glue_x86.h
  )
endif()

if(CORE_SYSTEM_NAME STREQUAL windowsstore)
  add_definitions(-D_WIN32_WINNT=0x0A00 -D_WINDOWS -D_CRT_NON_CONFORMING_SWPRINTFS -DNOMINMAX -D_WDL_CSTRING_IMPL_ONLY_)
else()
  add_definitions(-D_WIN32_WINNT=0x0600 -D_WINDOWS -DNOMINMAX -D_WDL_CSTRING_IMPL_ONLY_)
endif()
add_library(vis_milk2 STATIC ${SOURCES} ${SHADER_INCLUDES})
