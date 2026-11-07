# Compile production gameplay and presentation directly; only ESP-NOW hardware is replaced.
file(GLOB_RECURSE handheld_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
list(FILTER handheld_sources EXCLUDE REGEX "/(espnow_transport|game_session|party_registry|combat_rules|dungeontopology|puzzleinstructions|riddles|multiplayer_snapshot|network_protocol)\\.cpp$")
add_library(pfme_handheld_runtime STATIC ${handheld_sources} runtime.cpp no_radio.cpp controller_slots.cpp context_fields.cpp party_runtime.cpp native_worlds.cpp native_party.cpp audio_stream.cpp
    ../browser/third_party/adafruit_gfx/Adafruit_GFX.cpp)
target_include_directories(pfme_handheld_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(pfme_handheld_runtime PRIVATE ARDUINO=100 PUBLIC PFME_NATIVE_CONTROLLER_CONTEXT=1)
target_compile_features(pfme_handheld_runtime PRIVATE cxx_std_17)
target_link_libraries(pfme_handheld_runtime PUBLIC pfme_portable_leaf)
if(WIN32)
 target_link_libraries(pfme_handheld_runtime PUBLIC bcrypt)
endif()
add_executable(pfme_runtime_probe probe.cpp)
target_include_directories(pfme_runtime_probe PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
target_link_libraries(pfme_runtime_probe PRIVATE pfme_handheld_runtime)
if(BUILD_TESTING)
    add_executable(pfme_playthrough_tests playthrough_tests.cpp)
    target_include_directories(pfme_playthrough_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_playthrough_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_playthrough_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_production_playthrough COMMAND pfme_playthrough_tests 2)
    set_tests_properties(pfme_production_playthrough PROPERTIES TIMEOUT 30)

    add_executable(pfme_menu_render_tests menu_render_tests.cpp)
    target_include_directories(pfme_menu_render_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_menu_render_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_menu_render_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_menu_rendering COMMAND pfme_menu_render_tests)
    set_tests_properties(pfme_menu_rendering PROPERTIES TIMEOUT 30)

    add_executable(pfme_puzzle_entry_tests puzzle_entry_tests.cpp)
    target_include_directories(pfme_puzzle_entry_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_puzzle_entry_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_puzzle_entry_tests PRIVATE ARDUINO=100)
    foreach(puzzle bell number brazier riddleman)
        add_test(NAME pfme_puzzle_entry_${puzzle}
            COMMAND pfme_puzzle_entry_tests ${puzzle})
        set_tests_properties(pfme_puzzle_entry_${puzzle} PROPERTIES TIMEOUT 30)
    endforeach()

    add_executable(pfme_action_economy_runtime_tests action_economy_tests.cpp)
    target_include_directories(pfme_action_economy_runtime_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_action_economy_runtime_tests PRIVATE
        pfme_handheld_runtime)
    target_compile_definitions(pfme_action_economy_runtime_tests PRIVATE
        ARDUINO=100)
    add_test(NAME pfme_action_economy_runtime
        COMMAND pfme_action_economy_runtime_tests)
    set_tests_properties(pfme_action_economy_runtime PROPERTIES TIMEOUT 30)

    add_executable(pfme_save_slot_tests save_slot_tests.cpp)
    target_include_directories(pfme_save_slot_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_save_slot_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_save_slot_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_save_slots COMMAND pfme_save_slot_tests)
    set_tests_properties(pfme_save_slots PROPERTIES TIMEOUT 30)

    add_executable(pfme_spell_usability_tests spell_usability_tests.cpp)
    target_include_directories(pfme_spell_usability_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_spell_usability_tests PRIVATE
        pfme_handheld_runtime)
    target_compile_definitions(pfme_spell_usability_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_spell_usability COMMAND pfme_spell_usability_tests)
    set_tests_properties(pfme_spell_usability PROPERTIES TIMEOUT 30)

    add_executable(pfme_item_interaction_tests item_interaction_tests.cpp)
    target_include_directories(pfme_item_interaction_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_item_interaction_tests PRIVATE
        pfme_handheld_runtime)
    target_compile_definitions(pfme_item_interaction_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_item_interactions COMMAND pfme_item_interaction_tests)
    set_tests_properties(pfme_item_interactions PROPERTIES TIMEOUT 30)

    add_executable(pfme_party_item_authority_tests
        party_item_authority_tests.cpp)
    target_include_directories(pfme_party_item_authority_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_party_item_authority_tests PRIVATE
        pfme_handheld_runtime)
    target_compile_definitions(pfme_party_item_authority_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_party_item_authority
        COMMAND pfme_party_item_authority_tests)
    set_tests_properties(pfme_party_item_authority PROPERTIES TIMEOUT 60)

    add_executable(pfme_monster_balance_tests monster_balance_tests.cpp)
    target_include_directories(pfme_monster_balance_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_monster_balance_tests PRIVATE
        pfme_handheld_runtime)
    target_compile_definitions(pfme_monster_balance_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_monster_balance COMMAND pfme_monster_balance_tests)
    set_tests_properties(pfme_monster_balance PROPERTIES TIMEOUT 30)

    add_executable(pfme_action_pose_tests action_pose_tests.cpp)
    target_include_directories(pfme_action_pose_tests PRIVATE
        host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_action_pose_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_action_pose_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_action_pose COMMAND pfme_action_pose_tests)
    set_tests_properties(pfme_action_pose PROPERTIES TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(pfme_town_party_tests town_party_tests.cpp)
    target_include_directories(pfme_town_party_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_town_party_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_town_party_tests PRIVATE ARDUINO=100)
    foreach(size RANGE 1 4)
        add_test(NAME pfme_town_dungeon_${size} COMMAND pfme_town_party_tests ${size})
        add_test(NAME pfme_town_forest_${size} COMMAND pfme_town_party_tests ${size} forest)
    endforeach()
    foreach(size RANGE 1 4)
        add_test(NAME pfme_town_combat_${size} COMMAND pfme_town_party_tests ${size} combat)
        set_tests_properties(pfme_town_combat_${size} PROPERTIES TIMEOUT 120)
    endforeach()
    add_test(NAME pfme_town_forest_combat_3 COMMAND pfme_town_party_tests 3 forest-combat)
    add_test(NAME pfme_town_forest_combat_4 COMMAND pfme_town_party_tests 4 forest-combat)
    add_test(NAME pfme_town_forest_initiative_3 COMMAND pfme_town_party_tests 3 forest-initiative)
    add_test(NAME pfme_town_forest_initiative_4 COMMAND pfme_town_party_tests 4 forest-initiative)
    add_test(NAME pfme_town_separate_parties COMMAND pfme_town_party_tests 2 separate)
    add_executable(pfme_controller_tests controller_tests.cpp)
    target_include_directories(pfme_controller_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_controller_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_controller_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_controller_isolation COMMAND pfme_controller_tests)
endif()

if(BUILD_TESTING)
    add_executable(pfme_multicontroller_tests multicontroller_tests.cpp)
    target_include_directories(pfme_multicontroller_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_multicontroller_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_multicontroller_tests PRIVATE ARDUINO=100)
    foreach(party_size RANGE 1 4)
        add_test(NAME pfme_exploration_${party_size} COMMAND pfme_multicontroller_tests ${party_size})
    endforeach()
endif()

if(BUILD_TESTING)
    add_test(NAME pfme_party_mixed_classes COMMAND pfme_multicontroller_tests 4 mixed 2)
    add_test(NAME pfme_party_world_effects COMMAND pfme_multicontroller_tests 2 world-effects 2)
    add_test(NAME pfme_party_lifecycle COMMAND pfme_multicontroller_tests 4 lifecycle 2)
    add_test(NAME pfme_party_combat_1 COMMAND pfme_multicontroller_tests 1 combat 2)
    add_test(NAME pfme_party_combat_2 COMMAND pfme_multicontroller_tests 2 combat 0)
    add_test(NAME pfme_party_combat_3 COMMAND pfme_multicontroller_tests 3 combat 1)
    add_test(NAME pfme_party_combat_4 COMMAND pfme_multicontroller_tests 4 combat 2)
    set_tests_properties(pfme_exploration_1 pfme_exploration_2 pfme_exploration_3 pfme_exploration_4
        pfme_party_combat_1 pfme_party_combat_2 pfme_party_combat_3 pfme_party_combat_4
        PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
endif()

if(BUILD_TESTING)
    add_executable(pfme_audio_stream_tests audio_tests.cpp)
    target_include_directories(pfme_audio_stream_tests PRIVATE host ../browser/host ../browser/third_party/adafruit_gfx)
    target_link_libraries(pfme_audio_stream_tests PRIVATE pfme_handheld_runtime)
    target_compile_definitions(pfme_audio_stream_tests PRIVATE ARDUINO=100)
    add_test(NAME pfme_original_audio_stream COMMAND pfme_audio_stream_tests)
    set_tests_properties(pfme_original_audio_stream PROPERTIES TIMEOUT 60)
endif()
