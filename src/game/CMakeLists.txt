# bb_game: the game layer (migration.md, section 3.2). It depends on bb_core
# and the C++17 standard library only (no platform); it grows as the legacy game
# migrates (Phase 3: the screen stack; Phase 4: the world and entities).
add_library(bb_game STATIC
    src/content.cpp
    src/screen_stack.cpp
    src/screens/attract.cpp
    src/screens/campaign.cpp
    src/screens/character_select.cpp
    src/screens/credits.cpp
    src/screens/cutscene.cpp
    src/screens/dialogue.cpp
    src/screens/level_summary.cpp
    src/screens/menus.cpp
    src/screens/waits.cpp
    src/services.cpp
    src/world.cpp
    src/actor.cpp
    src/audio.cpp
    src/banner.cpp
    src/decor.cpp
    src/entity_registry.cpp
    src/effects.cpp
    src/game.cpp
    src/generators.cpp
    src/hero_choice.cpp
    src/hi_scores.cpp
    src/hud.cpp
    src/input.cpp
    src/finalboss/decor.cpp
    src/finalboss/mario.cpp
    src/finalboss/registration.cpp
    src/finalboss/stork.cpp
    src/gore.cpp
    src/level_events.cpp
    src/trig.cpp
    src/vehicle.cpp
    src/carebears/bears.cpp
    src/carebears/jedi_cameo.cpp
    src/carebears/registration.cpp
    src/carebears/shots.cpp
    src/carebears/zombie_shaman.cpp
    src/lemmings/lemmings.cpp
    src/level.cpp
    src/player/pickups.cpp
    src/player/player.cpp
    src/player/shots.cpp
    src/pokemon/decor.cpp
    src/pokemon/enemies.cpp
    src/pokemon/machines.cpp
    src/pokemon/registration.cpp
    src/smurf/big_smurf.cpp
    src/snorks/boss_shots.cpp
    src/snorks/registration.cpp
    src/snorks/troopers.cpp
    src/smurf/farmer_smurf.cpp
    src/smurf/greedy_smurf.cpp
    src/smurf/prankster_smurf.cpp
    src/smurf/registration.cpp
    src/smurf/smurf.cpp
    src/smurf/smurfette.cpp
    src/smurf/strongman_smurf.cpp
    src/smurf/wild_smurf.cpp
    src/video/decor.cpp
    src/video/princess.cpp
    src/video/rayman.cpp
    src/video/registration.cpp
    src/video/walkers.cpp
    src/videoboss/decor.cpp
    src/videoboss/lara.cpp
    src/videoboss/plumbers.cpp
    src/videoboss/registration.cpp
    src/videoboss/sonic.cpp
)
add_library(bb::game ALIAS bb_game)

target_include_directories(bb_game PUBLIC include PRIVATE src)
target_link_libraries(bb_game PUBLIC bb::core bb::platform PRIVATE bb::asset_ids)
target_compile_features(bb_game PUBLIC cxx_std_17)
bb_set_strict_warnings(bb_game)
