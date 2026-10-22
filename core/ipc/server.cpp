// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ipc/ipc.h"

#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace ThetaZ {

static void append_u32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

static uint32_t read_u32(const std::vector<uint8_t>& buffer, size_t offset) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4 && offset + i < buffer.size(); i++) {
        value |= static_cast<uint32_t>(buffer[offset + i]) << (i * 8);
    }
    return value;
}

IPCMessage::IPCMessage(IPCMessageType t, const std::vector<uint8_t>& d)
    : type(t), length(d.size()), request_id(0), data(d) {}

void IPCMessage::serialize(std::vector<uint8_t>& buffer) const {
    buffer.clear();
    buffer.reserve(sizeof(uint32_t) * 3 + data.size());
    append_u32(buffer, static_cast<uint32_t>(type));
    append_u32(buffer, length);
    append_u32(buffer, request_id);
    buffer.insert(buffer.end(), data.begin(), data.end());
}

IPCMessage IPCMessage::deserialize(const std::vector<uint8_t>& buffer) {
    IPCMessage msg;
    if (buffer.size() < sizeof(uint32_t) * 3) {
        return msg;
    }
    msg.type = static_cast<IPCMessageType>(read_u32(buffer, 0));
    msg.length = read_u32(buffer, 4);
    msg.request_id = read_u32(buffer, 8);
    if (buffer.size() >= 12 + msg.length) {
        msg.data.assign(buffer.begin() + 12, buffer.end());
    }
    return msg;
}

void IPCResponse::serialize(std::vector<uint8_t>& buffer) const {
    buffer.clear();
    buffer.reserve(sizeof(uint32_t) * 2 + data.size());
    append_u32(buffer, status);
    append_u32(buffer, length);
    buffer.insert(buffer.end(), data.begin(), data.end());
}

IPCResponse IPCResponse::deserialize(const std::vector<uint8_t>& buffer) {
    IPCResponse resp;
    if (buffer.size() < 8) {
        return resp;
    }
    resp.status = read_u32(buffer, 0);
    resp.length = read_u32(buffer, 4);
    if (buffer.size() >= 8 + resp.length) {
        resp.data.assign(buffer.begin() + 8, buffer.end());
    }
    return resp;
}

IPCServer::IPCServer() : running_(false), server_socket_(-1) {}

IPCServer::~IPCServer() {
    if (running_) {
        stop();
    }
}

void IPCServer::stop() {
    running_.store(false);
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

bool IPCServer::initialize(int port) {
    running_.store(false);

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    listen(server_socket_, 10);

    running_.store(true);
    server_thread_ = std::thread(&IPCServer::server_loop, this);

    return true;
}

void IPCServer::run() {
    if (running_.load()) {
        server_thread_.join();
    }
}

bool IPCServer::send_message(uint64_t client_id, const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> buffer;
    msg.serialize(buffer);
    auto it = pending_messages_.find(client_id);
    if (it == pending_messages_.end()) {
        return false;
    }
    it->second.insert(it->second.end(), buffer.begin(), buffer.end());
    return true;
}

void IPCServer::register_callback(IPCMessageType type, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_[type] = callback;
}

ProcessHandle* IPCServer::get_process(uint64_t pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(pid);
    if (it != processes_.end()) {
        return &it->second;
    }
    return nullptr;
}

ThreadHandle* IPCServer::get_thread(uint64_t tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = threads_.find(tid);
    if (it != threads_.end()) {
        return &it->second;
    }
    return nullptr;
}

uint64_t IPCServer::create_process(uint64_t pid, uint64_t ppid) {
    std::lock_guard<std::mutex> lock(mutex_);
    ProcessHandle handle{};
    handle.pid = pid;
    handle.ppid = ppid;
    handle.thread_id = pid;
    handle.is_critical = false;
    handle.priority_class = 0x20;
    processes_[pid] = handle;
    return pid;
}

uint64_t IPCServer::create_thread(uint64_t tid, uint64_t pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    ThreadHandle handle{};
    handle.tid = tid;
    handle.pid = pid;
    handle.is_running = true;
    handle.is_suspended = false;
    threads_[tid] = handle;
    return tid;
}

void IPCServer::server_loop() {
    while (running_.load()) {
        int client_socket = accept_client();
        if (client_socket < 0) {
            if (!running_.load()) break;
            continue;
        }
        std::thread client_thread(&IPCServer::handle_client, this, client_socket);
        client_thread.detach();
    }
}

int IPCServer::accept_client() {
    if (server_socket_ < 0) {
        return -1;
    }
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    int client_socket = accept(server_socket_,
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &addr_len);
    if (client_socket < 0) {
        return -1;
    }
    return client_socket;
}

void IPCServer::handle_client(int client_socket) {
    std::vector<uint8_t> buffer;
    buffer.reserve(64 * 1024);

    while (running_.load()) {
        if (!receive_data(client_socket, buffer)) {
            close(client_socket);
            break;
        }

        while (buffer.size() >= 12) {
            uint32_t type = read_u32(buffer, 0);
            uint32_t length = read_u32(buffer, 4);
            uint32_t request_id = read_u32(buffer, 8);

            if (buffer.size() < 12 + length) {
                break;
            }

            std::vector<uint8_t> data(buffer.begin() + 12, buffer.begin() + 12 + length);

            IPCMessage message(static_cast<IPCMessageType>(type), data);
            message.request_id = request_id;

            IPCResponse response;
            response.status = 0;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = callbacks_.find(message.type);
                if (it != callbacks_.end()) {
                    it->second(message, response);
                } else {
                    response.status = 1;
                }
            }

            std::vector<uint8_t> response_buffer;
            response.serialize(response_buffer);
            send_data(client_socket, response_buffer.data(), response_buffer.size());

            buffer.erase(buffer.begin(), buffer.begin() + 12 + length);
        }
    }
}

bool IPCServer::send_data(int socket, const uint8_t* data, size_t size) {
    size_t sent_total = 0;
    while (sent_total < size) {
        ssize_t sent = ::send(socket, data + sent_total, size - sent_total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (sent == 0) return false;
        sent_total += static_cast<size_t>(sent);
    }
    return true;
}

bool IPCServer::receive_data(int socket, std::vector<uint8_t>& buffer, size_t max_size) {
    if (buffer.size() >= max_size) return false;

    size_t remaining = max_size - buffer.size();
    size_t chunk = remaining < 4096 ? remaining : 4096;

    std::vector<uint8_t> temp(chunk);
    ssize_t received;
    do {
        received = ::recv(socket, temp.data(), temp.size(), 0);
    } while (received < 0 && errno == EINTR);

    if (received <= 0) return false;

    buffer.insert(buffer.end(), temp.begin(), temp.begin() + received);
    return true;
}

} // namespace ThetaZ
