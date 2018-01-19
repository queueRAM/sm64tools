cmake_minimum_required(VERSION 3.1)

project(sm64tools C)

if (MSVC)
   # Always static linking on windows
   add_definitions(/MT)
else ()
   add_definitions(-Wextra)
endif ()

set(CMAKE_C_FLAGS "${GCC_EXTRA_CFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${GCC_EXTRA_LDFLAGS}")

include_directories(${CMAKE_SOURCE_DIR}/ext)
include_directories("${PROJECT_SOURCE_DIR}/external/include")
link_directories("${PROJECT_SOURCE_DIR}/external/lib")

add_library(sm64 STATIC libmio0.c libsm64.c utils.c)

add_executable(sm64extend sm64extend.c)
target_link_libraries(sm64extend sm64)

add_executable(sm64compress sm64compress.c)
target_link_libraries(sm64compress sm64)

add_executable(sm64walk sm64walk.c)
target_link_libraries(sm64walk sm64)

add_executable(f3d f3d.c utils.c)

add_executable(f3d2obj blast.c f3d2obj.c n64graphics.c utils.c)
target_link_libraries(f3d2obj png z)

add_executable(sm64geo sm64geo.c utils.c)

add_executable(mio0 libmio0.c)
set_target_properties(mio0 PROPERTIES COMPILE_DEFINITIONS "MIO0_STANDALONE")

add_executable(mipsdisasm mipsdisasm.c utils.c yamlconfig.c)
set_target_properties(mipsdisasm PROPERTIES COMPILE_DEFINITIONS "MIPSDISASM_STANDALONE")
target_link_libraries(mipsdisasm capstone yaml)

add_executable(n64cksum n64cksum.c)
target_link_libraries(n64cksum sm64)

add_executable(n64graphics n64graphics.c utils.c)
set_target_properties(n64graphics PROPERTIES COMPILE_DEFINITIONS "N64GRAPHICS_STANDALONE")
target_link_libraries(n64graphics png z)

add_executable(n64split blast.c libsfx.c mipsdisasm.c n64split.c n64graphics.c strutils.c yamlconfig.c)
target_link_libraries(n64split sm64 capstone yaml z)

