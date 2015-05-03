cmake_minimum_required(VERSION 3.1)

project(sm64tools C)

if (MSVC)
    # Always static linking on windows
    add_definitions(/MT)
else ()
    add_definitions(-Wextra)
endif ()

include_directories("${PROJECT_SOURCE_DIR}/external/include")
link_directories("${PROJECT_SOURCE_DIR}/external/lib")

add_library(sm64 STATIC libmio0.c libsm64.c utils.c)

add_executable(sm64extend sm64extend.c)
target_link_libraries(sm64extend sm64)

add_executable(sm64compress sm64compress.c)
target_link_libraries(sm64compress sm64)

add_executable(f3d f3d.c)
target_link_libraries(f3d sm64)

add_executable(mio0 libmio0.c)
set_target_properties(mio0 PROPERTIES COMPILE_DEFINITIONS "MIO0_TEST")

add_executable(mipsdisasm config.c mipsdisasm.c utils.c)
set_target_properties(mipsdisasm PROPERTIES COMPILE_DEFINITIONS "MIPSDISASM_STANDALONE")
target_link_libraries(mipsdisasm capstone config)

add_executable(n64cksum n64cksum.c)
target_link_libraries(n64cksum sm64)

add_executable(n64graphics n64graphics.c utils.c)
set_target_properties(n64graphics PROPERTIES COMPILE_DEFINITIONS "N64GRAPHICS_STANDALONE")
target_link_libraries(n64graphics png z)

add_executable(n64split config.c mipsdisasm.c n64split.c n64graphics.c)
target_link_libraries(n64split sm64 capstone config png z)

