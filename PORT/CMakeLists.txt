cmake_minimum_required(VERSION 3.20)
project(rotr2port CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TP ${CMAKE_SOURCE_DIR}/thirdparty)
set(SDL2_DIR ${TP}/SDL2-2.30.11)
set(SDL2IMG_DIR ${TP}/SDL2_image-2.8.4)

add_executable(rotr2
  src/main.cpp
  src/assets.cpp
  src/fighter.cpp
  src/ui.cpp
  src/frontend.cpp
)

target_include_directories(rotr2 PRIVATE
  ${SDL2_DIR}/include
  ${SDL2IMG_DIR}/include
  ${TP}
  src
)

target_link_libraries(rotr2 PRIVATE
  ${SDL2_DIR}/lib/x64/SDL2.lib
  ${SDL2_DIR}/lib/x64/SDL2main.lib
  ${SDL2IMG_DIR}/lib/x64/SDL2_image.lib
)

# copier les DLL à côté de l'exécutable
add_custom_command(TARGET rotr2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_DIR}/lib/x64/SDL2.dll $<TARGET_FILE_DIR:rotr2>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2IMG_DIR}/lib/x64/SDL2_image.dll $<TARGET_FILE_DIR:rotr2>
)

include(CTest)
if(BUILD_TESTING)
  add_executable(rotr2_fighter_test tests/fighter_test.cpp src/fighter.cpp)
  target_include_directories(rotr2_fighter_test PRIVATE ${SDL2_DIR}/include src)
  add_test(NAME fighter_frames COMMAND rotr2_fighter_test)

  # Manual check against a private imported profile; no game data in CI.
  add_executable(rotr2_asset_smoke tests/asset_smoke.cpp
    src/assets.cpp src/fighter.cpp src/ui.cpp src/frontend.cpp)
  target_include_directories(rotr2_asset_smoke PRIVATE
    ${SDL2_DIR}/include ${SDL2IMG_DIR}/include ${TP} src)
  target_link_libraries(rotr2_asset_smoke PRIVATE
    ${SDL2_DIR}/lib/x64/SDL2.lib ${SDL2_DIR}/lib/x64/SDL2main.lib
    ${SDL2IMG_DIR}/lib/x64/SDL2_image.lib)
  add_custom_command(TARGET rotr2_asset_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_DIR}/lib/x64/SDL2.dll $<TARGET_FILE_DIR:rotr2_asset_smoke>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2IMG_DIR}/lib/x64/SDL2_image.dll $<TARGET_FILE_DIR:rotr2_asset_smoke>)
endif()
