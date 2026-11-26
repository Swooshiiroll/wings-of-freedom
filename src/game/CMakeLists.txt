add_executable(wof main.cpp)
set_target_properties(wof PROPERTIES OUTPUT_NAME "WingsOfFreedom")
target_compile_definitions(wof PRIVATE WOF_VERSION="${PROJECT_VERSION}")
target_link_libraries(wof PRIVATE wof_core SDL3::SDL3-static wof_warnings)

# tools/dev.sh build stages the game with: cmake --install <dir> --component game
install(TARGETS wof RUNTIME DESTINATION . COMPONENT game)
if(EXISTS ${PROJECT_SOURCE_DIR}/assets)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets DESTINATION . COMPONENT game)
endif()
