#include "engine/core/Application.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include "engine/input/InputSystem.h"
#include "engine/platform/Terminal.h"
#include "engine/render/FrameBuffer.h"
#include "engine/render/TerminalRenderer.h"

namespace engine {

Application::Application(AppConfig config) : config_(config) {}

int Application::run() {
    using Clock = std::chrono::steady_clock;

    platform::Terminal terminal;
    input::InputSystem input(terminal);
    render::TerminalRenderer renderer(terminal);
    render::FrameBuffer frame(0, 0);

    const auto frameBudget = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / config_.targetFps));

    auto previous = Clock::now();
    double accumulator = 0.0;

    // Game loop แบบ "Fix Your Timestep" (Glenn Fiedler):
    // render ตามความเร็วเครื่อง แต่ simulation เดินด้วย dt คงที่
    // => ผลลัพธ์ deterministic ไม่ขึ้นกับ FPS (สำคัญมากสำหรับ multiplayer / replay)
    while (true) {
        const auto frameStart = Clock::now();

        scenes_.applyPending();
        scene::Scene* scene = scenes_.current();
        if (!scene) break;

        // 1) Input
        const input::InputMode mode = scene->wantsTextInput() ? input::InputMode::Text : input::InputMode::Game;
        const input::InputFrame inputFrame = input.poll(mode);
        if (inputFrame.has(input::Action::Quit)) break;
        scene->handleInput(inputFrame);

        // 2) Update (fixed timestep)
        const double elapsed = std::chrono::duration<double>(frameStart - previous).count();
        previous = frameStart;
        accumulator += std::min(elapsed, config_.maxFrameTime);
        while (accumulator >= config_.fixedDt) {
            scene->update(config_.fixedDt);
            accumulator -= config_.fixedDt;
        }

        // 3) Render
        const Vec2i size = terminal.size();
        if (frame.width() != size.x || frame.height() != size.y) {
            frame = render::FrameBuffer(size.x, size.y);
        }
        frame.clear();
        scene->render(frame);
        renderer.present(frame);

        // 4) รอให้ครบ frame budget ไม่ให้กิน CPU 100%
        std::this_thread::sleep_until(frameStart + frameBudget);
    }
    return 0;
}

}  // namespace engine
