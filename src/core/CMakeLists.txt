# Pure C++20 simulation core: no SDL, no I/O. Unit tests link only this.
add_library(wof_core STATIC
  camera.cpp
  fighter.cpp
  fixed_step.cpp
  flight.cpp
  ground.cpp
  raster.cpp
  tuning.cpp
  view.cpp
  world.cpp)
target_include_directories(wof_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(wof_core PUBLIC cxx_std_20)
target_link_libraries(wof_core PRIVATE wof_warnings)
