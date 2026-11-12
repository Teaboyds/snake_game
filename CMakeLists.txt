cmake_minimum_required(VERSION 3.16)
project(SnakeAAA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

function(enable_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

# ---------- Engine layer: ไม่รู้จักเกมงูเลย นำไปใช้กับเกมอื่นได้ ----------
add_library(engine STATIC
    src/engine/core/Application.cpp
    src/engine/platform/Terminal.cpp
    src/engine/input/InputSystem.cpp
    src/engine/render/FrameBuffer.cpp
    src/engine/render/TerminalRenderer.cpp
    src/engine/scene/SceneManager.cpp
)
target_include_directories(engine PUBLIC src)
enable_warnings(engine)

# ---------- Backend layer: client ของ service ภายนอก (ไม่รู้จักเกมงู) ----------
add_library(backend STATIC
    src/backend/redis/RedisClient.cpp
)
target_include_directories(backend PUBLIC src)
enable_warnings(backend)

# ---------- Game layer: กฎของเกมงู + หน้าจอต่าง ๆ ----------
add_library(game STATIC
    src/game/sim/Snake.cpp
    src/game/sim/World.cpp
    src/game/services/InMemoryLeaderboard.cpp
    src/game/services/RedisLeaderboard.cpp
    src/game/scenes/LeaderboardView.cpp
    src/game/scenes/MenuScene.cpp
    src/game/scenes/NameEntryScene.cpp
    src/game/scenes/PlayScene.cpp
    src/game/scenes/GameOverScene.cpp
)
target_link_libraries(game PUBLIC engine backend)
enable_warnings(game)

# ---------- Executable ----------
add_executable(snake src/main.cpp)
target_link_libraries(snake PRIVATE game)
enable_warnings(snake)

# ---------- Tests (ทดสอบ simulation โดยไม่ต้องเปิด terminal) ----------
enable_testing()
add_executable(snake_tests tests/TestMain.cpp)
target_link_libraries(snake_tests PRIVATE game)
enable_warnings(snake_tests)
add_test(NAME snake_tests COMMAND snake_tests)
