// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>

namespace ThetaZ {

enum class IPCMessageType : uint32_t {
    None = 0,
    CreateProcess = 1,
    CreateThread = 2,
    GetProcess = 3,
    GetThread = 4,
    CreateWindow = 5,
    SendMessage = 6,
    RegisterEvent = 7,
    GetEvent = 8,
    GetTickCount = 9,
    QueryInformationProcess = 10,
    QueryInformationThread = 11,
    TerminateProcess = 12,
    TerminateThread = 13
};

struct IPCMessage {
    IPCMessageType type;
    uint32_t length;
    uint32_t request_id;
    std::vector<uint8_t> data;

    IPCMessage() : type(IPCMessageType::None), length(0), request_id(0) {}
    IPCMessage(IPCMessageType t, const std::vector<uint8_t>& d = {});

    void serialize(std::vector<uint8_t>& buffer) const;
    static IPCMessage deserialize(const std::vector<uint8_t>& buffer);
};

struct IPCResponse {
    uint32_t status;
    uint32_t length;
    std::vector<uint8_t> data;

    IPCResponse() : status(0), length(0) {}
    void serialize(std::vector<uint8_t>& buffer) const;
    static IPCResponse deserialize(const std::vector<uint8_t>& buffer);
};

struct ProcessHandle {
    uint64_t pid;
    uint64_t ppid;
    uint64_t thread_id;
    bool is_critical;
    uint32_t priority_class;
};

struct ThreadHandle {
    uint64_t tid;
    uint64_t pid;
    uint64_t stack_base;
    uint64_t stack_limit;
    uint64_t exit_code;
    bool is_running;
    bool is_suspended;
};

struct WindowHandle {
    uint64_t hwnd;
    uint64_t parent;
    uint64_t next;
    uint64_t prev;
    bool visible;
    bool enabled;
    uint32_t style;
    uint32_t extended_style;
};

class IPCServer {
public:
    IPCServer();
    ~IPCServer();

    bool initialize(int port);
    void run();
    bool send_message(uint64_t client_id, const IPCMessage& msg);

    using MessageCallback = std::function<void(const IPCMessage&, IPCResponse&)>;
    void register_callback(IPCMessageType type, MessageCallback callback);

    ProcessHandle* get_process(uint64_t pid);
    ThreadHandle* get_thread(uint64_t tid);

    uint64_t create_process(uint64_t pid, uint64_t ppid);
    uint64_t create_thread(uint64_t tid, uint64_t pid);

    bool is_running() const { return running_.load(); }
    void stop();

private:
    void server_loop();
    int accept_client();
    void handle_client(int client_socket);
    bool send_data(int socket, const uint8_t* data, size_t size);
    bool receive_data(int socket, std::vector<uint8_t>& buffer, size_t max_size = 64 * 1024);

    std::atomic<bool> running_;
    int server_socket_;
    std::thread server_thread_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> pending_messages_;
    std::unordered_map<uint64_t, ProcessHandle> processes_;
    std::unordered_map<uint64_t, ThreadHandle> threads_;
    std::unordered_map<IPCMessageType, MessageCallback> callbacks_;
    mutable std::mutex mutex_;
};

} // namespace ThetaZ
