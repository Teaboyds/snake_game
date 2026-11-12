#include "engine/platform/Terminal.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace engine::platform {
namespace {

termios g_original{};
bool g_rawEnabled = false;
volatile std::sig_atomic_t g_quitSignal = 0;

void restoreTerminal() {
    if (!g_rawEnabled) return;
    // reset สี, โชว์ cursor, ออกจาก alternate screen
    const char* seq = "\x1b[0m\x1b[?25h\x1b[?1049l";
    (void)::write(STDOUT_FILENO, seq, std::strlen(seq));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_original);
    g_rawEnabled = false;
}

void onSignal(int) { g_quitSignal = 1; }

}  // namespace

Terminal::Terminal() {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        throw std::runtime_error("must be run in an interactive terminal");
    }
    if (tcgetattr(STDIN_FILENO, &g_original) != 0) {
        throw std::runtime_error("tcgetattr failed");
    }

    termios raw = g_original;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);  // ไม่รอ Enter, ไม่ echo, Ctrl+C มาเป็น byte
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;   // read() คืนทันทีแม้ไม่มีข้อมูล
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        throw std::runtime_error("tcsetattr failed");
    }
    g_rawEnabled = true;

    std::atexit(restoreTerminal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGHUP, onSignal);

    // alternate screen, ซ่อน cursor, ล้างจอ
    write("\x1b[?1049h\x1b[?25l\x1b[2J");
}

Terminal::~Terminal() { restoreTerminal(); }

void Terminal::write(std::string_view data) {
    while (!data.empty()) {
        const ssize_t n = ::write(STDOUT_FILENO, data.data(), data.size());
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return;
        data.remove_prefix(static_cast<size_t>(n));
    }
}

std::string Terminal::readAvailable() {
    std::string out;
    char buf[64];
    while (true) {
        const ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) break;
        out.append(buf, static_cast<size_t>(n));
    }
    return out;
}

Vec2i Terminal::size() const {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {ws.ws_col, ws.ws_row};
    }
    return {80, 24};
}

bool Terminal::quitRequested() const { return g_quitSignal != 0; }

}  // namespace engine::platform
