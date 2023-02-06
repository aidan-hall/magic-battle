# CMake config
cmake_minimum_required(VERSION 3.15)

project(MagicBattle VERSION 0.1 LANGUAGES "ASM" "CXX")
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Executable
add_executable(MagicBattle source/main.cpp source/ndspp.cpp source/systems.cpp source/util.cpp)

target_include_directories(MagicBattle PUBLIC include ${CMAKE_CURRENT_BINARY_DIR})

target_compile_options(MagicBattle PRIVATE
  -Wpedantic
  -Wall
  -Wextra
  -Wimplicit-fallthrough
  -Wno-volatile
  $<$<CONFIG:Release>:-Werror>
  $<$<CONFIG:Debug>:-g3>
  $<$<CONFIG:Debug>:-Og>
  $<$<CONFIG:Release>:-O3>
  -g
)

# Libraries

# Images
grit_add_binary_target(PlayerSprite sprites/player.png DEPTH 8 NO_MAP OPTIONS -Mh 2 -Mw 2)
grit_add_binary_target(ZombieSprite sprites/zombie.png DEPTH 8 NO_MAP OPTIONS -Mh 2 -Mw 2)
grit_add_binary_target(FireballSprite sprites/fireball.png DEPTH 8 NO_MAP OPTIONS)
grit_add_binary_target(ExplosionSprite sprites/explosion.png DEPTH 8 NO_MAP OPTIONS -Mh 8 -Mw 8)

dkp_add_embedded_binary_library(Sprites PlayerSprite ZombieSprite FireballSprite ExplosionSprite)

target_link_libraries(MagicBattle PUBLIC Sprites)

# Sound

mm_add_soundbank_target(Sounds HEADER soundbank.h INPUTS sounds/explosion.wav sounds/teleport.wav sounds/hit.wav sounds/fireball.wav)
dkp_add_embedded_binary_library(SoundsLib Sounds)
target_link_libraries(MagicBattle PUBLIC SoundsLib)
target_link_libraries(MagicBattle PUBLIC "-lmm9")

# Entity Component System
add_subdirectory(external/tecs)
target_link_libraries(MagicBattle PUBLIC tecs)

# Make the NDS file!
nds_create_rom(MagicBattle)

file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)
