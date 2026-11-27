# Unit tests: core library only, so they run headless (no window, no GPU) locally and on CI.
add_executable(wof_tests
  test_main.cpp
  test_menu.cpp
  test_ai.cpp
  test_bombers.cpp
  test_planes.cpp
  test_camera.cpp
  test_dashboard.cpp
  test_damage.cpp
  test_fighter.cpp
  test_flak.cpp
  test_fixed_step.cpp
  test_flight.cpp
  test_ground.cpp
  test_iso.cpp
  test_rng.cpp
  test_settings.cpp
  test_targets.cpp
  test_tree.cpp
  test_tuning.cpp
  test_view.cpp
  test_waves.cpp
  test_weapons.cpp
  test_world.cpp)
target_link_libraries(wof_tests PRIVATE wof_core doctest::doctest wof_warnings)
add_test(NAME core COMMAND wof_tests)
