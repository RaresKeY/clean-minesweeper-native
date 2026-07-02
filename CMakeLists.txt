cmake_minimum_required(VERSION 3.16)

project(clean_mines VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CLEAN_MINES_WITH_SDL "Build the SDL2 graphical frontend when SDL2 is available" ON)
option(CLEAN_MINES_REQUIRE_SDL "Fail configuration if the SDL2 graphical frontend is unavailable" OFF)

add_executable(clean-mines src/main.c)

set(CLEAN_MINES_HAS_SDL OFF)
if(CLEAN_MINES_WITH_SDL)
  find_package(SDL2 QUIET CONFIG)
  if(NOT TARGET SDL2::SDL2)
    find_package(SDL2 QUIET)
  endif()

  if(TARGET SDL2::SDL2)
    target_link_libraries(clean-mines PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
      target_link_libraries(clean-mines PRIVATE SDL2::SDL2main)
    endif()
    set(CLEAN_MINES_HAS_SDL ON)
  elseif(SDL2_FOUND)
    target_include_directories(clean-mines PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(clean-mines PRIVATE ${SDL2_LIBRARIES})
    set(CLEAN_MINES_HAS_SDL ON)
  endif()

  if(CLEAN_MINES_HAS_SDL)
    target_compile_definitions(clean-mines PRIVATE CLEAN_MINES_WITH_SDL=1)
  elseif(CLEAN_MINES_REQUIRE_SDL)
    message(FATAL_ERROR "SDL2 was requested but was not found")
  else()
    message(STATUS "SDL2 not found; building terminal-only clean-mines")
  endif()
endif()

include(GNUInstallDirs)

install(TARGETS clean-mines RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/clean-mines)
install(FILES docs/clean-mines.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
install(DIRECTORY assets/native/ DESTINATION ${CMAKE_INSTALL_DATADIR}/clean-mines/assets FILES_MATCHING PATTERN "*.bmp")

if(MSVC)
  target_compile_options(clean-mines PRIVATE /W4)
else()
  target_compile_options(clean-mines PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME self_test COMMAND clean-mines --self-test)
