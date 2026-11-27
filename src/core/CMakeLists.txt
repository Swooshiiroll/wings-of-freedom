# Pure C++20 simulation core: no SDL, no I/O. Unit tests link only this.
add_library(wof_core STATIC
  ai.cpp
  camera.cpp
  canvas.cpp
  damage.cpp
  dashboard.cpp
  fighter.cpp
  fixed_step.cpp
  flight.cpp
  ground.cpp
  gunners.cpp
  hangar.cpp
  menu.cpp
  pixel_font.cpp
  plane_rules.cpp
  planes.cpp
  progress.cpp
  raster.cpp
  score.cpp
  settings.cpp
  targets.cpp
  tuning.cpp
  upgrades.cpp
  view.cpp
  waves.cpp
  weapons.cpp
  wireframe.cpp
  world.cpp)
target_include_directories(wof_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(wof_core PUBLIC cxx_std_20)
target_link_libraries(wof_core PRIVATE wof_warnings)
