project(nseel LANGUAGES C ASM_NASM)

cmake_minimum_required(VERSION 3.5)

add_library(nseel STATIC
    asm-nseel-x64.asm
    eel_atomic.h
    eel_eval.h
    eel_fft.h
    eel_files.h
    eel_lice.h
    eel_mdct.h
    eel_misc.h
    eel_net.h
    eel_strings.h
    eelscript.h
    glue_x86_64.h
    ns-eel-addfuncs.h
    ns-eel-func-ref.h
    ns-eel-int.h
    ns-eel.h
    nseel-caltab.c
    nseel-cfunc.c
    nseel-compiler.c
    nseel-eval.c
    nseel-lextab.c
    nseel-ram.c
    nseel-yylex.c
)