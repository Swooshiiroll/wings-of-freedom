add_executable(wof
  bindings.cpp
  dashboard_view.cpp
  effects.cpp
  ground_view.cpp
  hangar_view.cpp
  plane_sprites.cpp
  main.cpp
  menu_view.cpp)
set_target_properties(wof PROPERTIES OUTPUT_NAME "WingsOfFreedom")
target_compile_definitions(wof PRIVATE WOF_VERSION="${PROJECT_VERSION}")
find_package(Threads REQUIRED) # the sprites bake on a background thread
target_link_libraries(wof PRIVATE wof_core SDL3::SDL3-static Threads::Threads wof_warnings)

# tools/dev.sh build stages the game with: cmake --install <dir> --component game
install(TARGETS wof RUNTIME DESTINATION . COMPONENT game)
if(EXISTS ${PROJECT_SOURCE_DIR}/assets)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets DESTINATION . COMPONENT game)
endif()
