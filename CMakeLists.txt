# ornament-player; play Ogg Theora files on Olive's christmas ornament.
#
# Please see the file LICENSE.txt in the source's root directory.
#
# This file written by Ryan C. Gordon.

cmake_minimum_required(VERSION 3.12.0)

project(ornament-player)

find_package(SDL3 REQUIRED)

add_library(ornament-ogg STATIC
	thirdparty/libogg/src/bitwise.c
	thirdparty/libogg/src/framing.c
)
target_include_directories(ornament-ogg PRIVATE
    thirdparty/libogg/include
)

add_library(ornament-vorbis STATIC
    thirdparty/libvorbis/lib/analysis.c
    thirdparty/libvorbis/lib/bitrate.c
    thirdparty/libvorbis/lib/block.c
    thirdparty/libvorbis/lib/codebook.c
    thirdparty/libvorbis/lib/envelope.c
    thirdparty/libvorbis/lib/floor0.c
    thirdparty/libvorbis/lib/floor1.c
    thirdparty/libvorbis/lib/info.c
    thirdparty/libvorbis/lib/lpc.c
    thirdparty/libvorbis/lib/lsp.c
    thirdparty/libvorbis/lib/mapping0.c
    thirdparty/libvorbis/lib/mdct.c
    thirdparty/libvorbis/lib/psy.c
    thirdparty/libvorbis/lib/registry.c
    thirdparty/libvorbis/lib/res0.c
    thirdparty/libvorbis/lib/sharedbook.c
    thirdparty/libvorbis/lib/smallft.c
    thirdparty/libvorbis/lib/synthesis.c
    thirdparty/libvorbis/lib/vorbisenc.c
    thirdparty/libvorbis/lib/vorbisfile.c
    thirdparty/libvorbis/lib/window.c
)
target_include_directories(ornament-vorbis PRIVATE
    thirdparty/libogg/include
    thirdparty/libvorbis/include
    thirdparty/libvorbis/lib
)

add_library(ornament-theora STATIC
    thirdparty/libtheora/lib/apiwrapper.c
    thirdparty/libtheora/lib/bitpack.c
    thirdparty/libtheora/lib/decapiwrapper.c
    thirdparty/libtheora/lib/decinfo.c
    thirdparty/libtheora/lib/decode.c
    thirdparty/libtheora/lib/dequant.c
    thirdparty/libtheora/lib/fragment.c
    thirdparty/libtheora/lib/huffdec.c
    thirdparty/libtheora/lib/idct.c
    thirdparty/libtheora/lib/info.c
    thirdparty/libtheora/lib/internal.c
    thirdparty/libtheora/lib/quant.c
    thirdparty/libtheora/lib/state.c
)
target_include_directories(ornament-theora PRIVATE
    thirdparty/libogg/include
    thirdparty/libtheora/include
)

add_library(ornament-theoraplay STATIC
    thirdparty/theoraplay/theoraplay.c
)
target_include_directories(ornament-theoraplay PRIVATE
    thirdparty/libogg/include
    thirdparty/libvorbis/include
    thirdparty/libtheora/include
)
target_compile_definitions(ornament-theoraplay PRIVATE "-DTHEORAPLAY_NO_MALLOC_FALLBACK=1")
target_compile_definitions(ornament-theoraplay PRIVATE "-DTHEORAPLAY_NO_FOPEN_FALLBACK=1")


add_executable(ornament ornament.c)
target_include_directories(ornament PRIVATE
    thirdparty/theoraplay
)
target_link_libraries(ornament SDL3::SDL3 ornament-theoraplay ornament-theora ornament-vorbis ornament-ogg)
target_link_libraries(ornament pthread m)

# end of CMakeLists.txt ...

