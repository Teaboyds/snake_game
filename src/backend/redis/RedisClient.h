#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sockaddr;

namespace backend::redis {

// คำตอบจาก Redis ตามโปรโตคอล RESP2
//   +OK          -> Status        -ERR ...  -> Error
//   :42          -> Integer       $5 hello  -> Bulk
//   $-1 / *-1    -> Nil           *2 ...    -> Array
struct Reply {
    enum class Type { Status, Error, Integer, Bulk, Nil, Array };

    Type type = Type::Nil;
    std::string str;  // Status / Error / Bulk
    long long integer = 0;
    std::vector<Reply> elements;  // Array

    bool isError() const { return type == Type::Error; }
    bool isNil() const { return type == Type::Nil; }
};

// แปลง command เป็น RESP: ทุก command คือ Array ของ Bulk String
// {"SET", "k", "v"} -> "*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$1\r\nv\r\n"
std::string encodeCommand(const std::vector<std::string>& args);

// อ่าน reply 1 ตัวจากต้น data คืน nullopt ถ้าข้อมูลยังมาไม่ครบ (ต้องรอ recv เพิ่ม)
// consumed = จำนวน byte ที่ใช้ไป, throw std::runtime_error ถ้าข้อมูลผิดรูปแบบ
std::optional<Reply> parseReply(std::string_view data, std::size_t& consumed);

// Redis client แบบ blocking ขนาดเล็ก (TCP socket + RESP)
// ทุก I/O มี timeout เกมจะไม่ค้างถ้า Redis ล่ม
class Client {
public:
    Client(std::string host, int port, std::chrono::milliseconds timeout);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect();
    void disconnect();
    bool connected() const { return fd_ >= 0; }

    std::optional<Reply> command(const std::vector<std::string>& args);

    // Pipelining: ส่งทุก command ไปทีเดียว แล้วค่อยอ่านคำตอบทั้งหมด = 1 round trip
    std::optional<std::vector<Reply>> pipeline(const std::vector<std::vector<std::string>>& commands);

    const std::string& host() const { return host_; }
    int port() const { return port_; }
    const std::string& lastError() const { return lastError_; }

private:
    bool connectWithTimeout(int fd, const sockaddr* addr, unsigned addrLen);
    bool sendAll(std::string_view data);
    bool receiveMore();

    std::string host_;
    int port_;
    std::chrono::milliseconds timeout_;
    int fd_ = -1;
    std::string buffer_;  // byte ที่ recv มาแล้วแต่ยัง parse ไม่หมด
    std::string lastError_;
};

}  // namespace backend::redis
