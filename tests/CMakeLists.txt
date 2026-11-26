# Unit tests: core library only, so they run headless (no window, no GPU) locally and on CI.
add_executable(wof_tests
  test_main.cpp
  test_fixed_step.cpp
  test_rng.cpp)
target_link_libraries(wof_tests PRIVATE wof_core doctest::doctest wof_warnings)
add_test(NAME core COMMAND wof_tests)
