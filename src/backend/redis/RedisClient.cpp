#include "backend/redis/RedisClient.h"

#include <cerrno>
#include <charconv>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace backend::redis {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;  // Linux: ไม่ให้ SIGPIPE ฆ่าโปรแกรมตอน Redis ปิด connection
#else
constexpr int kSendFlags = 0;  // macOS ใช้ SO_NOSIGPIPE แทน (ตั้งตอน connect)
#endif

long long parseInteger(std::string_view s) {
    long long value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) {
        throw std::runtime_error("redis protocol error: bad integer '" + std::string(s) + "'");
    }
    return value;
}

std::optional<Reply> parseAt(std::string_view data, std::size_t& pos) {
    const std::size_t eol = data.find("\r\n", pos);
    if (pos >= data.size() || eol == std::string_view::npos) return std::nullopt;

    const char type = data[pos];
    const std::string_view line = data.substr(pos + 1, eol - pos - 1);
    const std::size_t next = eol + 2;
    Reply reply;

    switch (type) {
        case '+':
            reply.type = Reply::Type::Status;
            reply.str = line;
            pos = next;
            return reply;
        case '-':
            reply.type = Reply::Type::Error;
            reply.str = line;
            pos = next;
            return reply;
        case ':':
            reply.type = Reply::Type::Integer;
            reply.integer = parseInteger(line);
            pos = next;
            return reply;
        case '$': {
            const long long length = parseInteger(line);
            if (length < 0) {
                pos = next;
                return reply;  // Nil
            }
            const auto len = static_cast<std::size_t>(length);
            if (data.size() < next + len + 2) return std::nullopt;
            reply.type = Reply::Type::Bulk;
            reply.str = data.substr(next, len);
            pos = next + len + 2;
            return reply;
        }
        case '*': {
            const long long count = parseInteger(line);
            if (count < 0) {
                pos = next;
                return reply;  // Nil
            }
            reply.type = Reply::Type::Array;
            std::size_t cursor = next;
            for (long long i = 0; i < count; ++i) {
                std::optional<Reply> element = parseAt(data, cursor);
                if (!element) return std::nullopt;
                reply.elements.push_back(std::move(*element));
            }
            pos = cursor;
            return reply;
        }
        default:
            throw std::runtime_error(std::string("redis protocol error: unknown type '") + type + "'");
    }
}

}  // namespace

std::string encodeCommand(const std::vector<std::string>& args) {
    std::string out = "*" + std::to_string(args.size()) + "\r\n";
    for (const std::string& arg : args) {
        out += "$" + std::to_string(arg.size()) + "\r\n";
        out += arg;
        out += "\r\n";
    }
    return out;
}

std::optional<Reply> parseReply(std::string_view data, std::size_t& consumed) {
    std::size_t pos = 0;
    std::optional<Reply> reply = parseAt(data, pos);
    if (reply) consumed = pos;
    return reply;
}

Client::Client(std::string host, int port, std::chrono::milliseconds timeout)
    : host_(std::move(host)), port_(port), timeout_(timeout) {}

Client::~Client() { disconnect(); }

bool Client::connect() {
    disconnect();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const int rc = ::getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &results);
    if (rc != 0) {
        lastError_ = ::gai_strerror(rc);
        return false;
    }

    for (addrinfo* ai = results; ai != nullptr && fd_ < 0; ai = ai->ai_next) {
        const int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connectWithTimeout(fd, ai->ai_addr, ai->ai_addrlen)) {
            fd_ = fd;
        } else {
            ::close(fd);
        }
    }
    ::freeaddrinfo(results);
    if (fd_ < 0) return false;

    timeval tv{};
    tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout_.count() / 1000);
    tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout_.count() % 1000) * 1000);
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    const int one = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));  // ส่งทันที ไม่รอรวม packet (Nagle)
#ifdef SO_NOSIGPIPE
    ::setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    return true;
}

bool Client::connectWithTimeout(int fd, const sockaddr* addr, unsigned addrLen) {
    // connect() ปกติอาจค้างนานหลายวินาที จึงทำแบบ non-blocking แล้วรอด้วย poll() ตาม timeout
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (::connect(fd, addr, static_cast<socklen_t>(addrLen)) != 0) {
        if (errno != EINPROGRESS) {
            lastError_ = std::strerror(errno);
            return false;
        }
        pollfd pfd{fd, POLLOUT, 0};
        const int ready = ::poll(&pfd, 1, static_cast<int>(timeout_.count()));
        if (ready <= 0) {
            lastError_ = ready == 0 ? "connect timeout" : std::strerror(errno);
            return false;
        }
        int error = 0;
        socklen_t len = sizeof(error);
        ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            lastError_ = std::strerror(error);
            return false;
        }
    }

    ::fcntl(fd, F_SETFL, flags);  // กลับเป็น blocking (+ timeout จาก SO_RCVTIMEO/SO_SNDTIMEO)
    return true;
}

void Client::disconnect() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    buffer_.clear();
}

std::optional<Reply> Client::command(const std::vector<std::string>& args) {
    std::optional<std::vector<Reply>> replies = pipeline({args});
    if (!replies) return std::nullopt;
    return std::move(replies->front());
}

std::optional<std::vector<Reply>> Client::pipeline(const std::vector<std::vector<std::string>>& commands) {
    if (fd_ < 0) {
        lastError_ = "not connected";
        return std::nullopt;
    }

    std::string out;
    for (const auto& args : commands) out += encodeCommand(args);
    if (!sendAll(out)) {
        disconnect();
        return std::nullopt;
    }

    std::vector<Reply> replies;
    try {
        while (replies.size() < commands.size()) {
            std::size_t consumed = 0;
            if (std::optional<Reply> reply = parseReply(buffer_, consumed)) {
                buffer_.erase(0, consumed);
                replies.push_back(std::move(*reply));
            } else if (!receiveMore()) {
                disconnect();
                return std::nullopt;
            }
        }
    } catch (const std::exception& e) {
        lastError_ = e.what();
        disconnect();
        return std::nullopt;
    }
    return replies;
}

bool Client::sendAll(std::string_view data) {
    while (!data.empty()) {
        const ssize_t n = ::send(fd_, data.data(), data.size(), kSendFlags);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            lastError_ = n == 0 ? "connection closed" : std::strerror(errno);
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

bool Client::receiveMore() {
    char chunk[4096];
    while (true) {
        const ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0);
        if (n > 0) {
            buffer_.append(chunk, static_cast<std::size_t>(n));
            return true;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n == 0) {
            lastError_ = "connection closed by server";
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            lastError_ = "read timeout";
        } else {
            lastError_ = std::strerror(errno);
        }
        return false;
    }
}

}  // namespace backend::redis
