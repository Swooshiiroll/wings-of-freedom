# Unit tests: core library only, so they run headless (no window, no GPU) locally and on CI.
add_executable(wof_tests
  test_main.cpp
  test_camera.cpp
  test_fixed_step.cpp
  test_ground.cpp
  test_iso.cpp
  test_rng.cpp
  test_tuning.cpp
  test_view.cpp
  test_world.cpp)
target_link_libraries(wof_tests PRIVATE wof_core doctest::doctest wof_warnings)
add_test(NAME core COMMAND wof_tests)
