cmake_minimum_required(VERSION 3.15)

project(MagicBattle VERSION 0.1 LANGUAGES "ASM" "CXX")
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

grit_add_binary_target(AlienSprite sprites/alien1.png DEPTH 8 NO_MAP OPTIONS -Mh 4 -Mw 4)
grit_add_binary_target(PlayerSprite sprites/player.png DEPTH 8 NO_MAP OPTIONS -Mh 4 -Mw 4 -W3)

dkp_add_embedded_binary_library(Sprites AlienSprite PlayerSprite)

add_executable(MagicBattle source/main.cpp source/ndspp.cpp)

target_compile_options(MagicBattle PRIVATE -Wpedantic -Wall -Wextra -Wimplicit-fallthrough -Wno-volatile)
target_include_directories(MagicBattle PUBLIC include)

target_link_libraries(MagicBattle PUBLIC Sprites)

add_subdirectory(external/tecs)
target_link_libraries(MagicBattle PUBLIC tecs)

nds_create_rom(MagicBattle)

file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)
