// Unix domain socket IPC server with optional HMAC challenge-response auth.
// Protocol: cmd:<name> [key=value ...]\n
// Auth: when WIBWOB_AUTH_SECRET is set, new connections must complete a
//       challenge-response handshake before commands are accepted.
#pragma once

#include <string>
#include <set>
#include <vector>
#include <cstdint>
#include <chrono>
#include <atomic>
#include <thread>

class TWwdosApp;

class ApiIpcServer {
public:
    /// Connection state for the status indicator.
    struct ConnectionStatus {
        bool listening   = false;  ///< Socket is open and accepting connections
        bool api_active  = false;  ///< API server sent a command recently (< 10 s)
        int  client_count = 0;     ///< Number of persistent event subscribers
    };

    explicit ApiIpcServer(TWwdosApp* app);
    ~ApiIpcServer();

    // Start listening on a Unix socket path. Returns false on failure.
    bool start(const std::string& path = "/tmp/wwdos.sock");
    // Serve every waiting connection (one command each; `batch` carries many).
    void poll();
    // Stop and clean up.
    void stop();

    // Run one command line ("cmd:name k=v ...") as if it came on the socket, without one:
    // the residents' own hands inside the app (resident_hands.cpp) use it.
    std::string runLine(const std::string& line) { std::string cmd; return dispatch(line, cmd); }

    // Push a newline-delimited JSON event to all active subscribers.
    // Cleans up disconnected subscriber fds automatically.
    void publish_event(const char* event_type, const std::string& payload_json = "{}");

    /// Query current connection state (thread-safe read of atomic-like fields).
    ConnectionStatus getConnectionStatus() const;

private:
    TWwdosApp* app_ = nullptr;
    int fd_listen_ = -1;
    std::string sock_path_;
    std::string auth_secret_;       // from WIBWOB_AUTH_SECRET env var (empty = no auth)
    std::set<std::string> used_nonces_;  // replay protection

    // Event push: persistent subscriber connections
    std::vector<int> event_subscribers_;
    uint64_t next_event_seq_ = 1;

    // person_wait connections held until a gesture arrives (resident_senses.h, gestures).
    struct Waiter {
        int fd = -1;
        uint64_t since = 0;
        int limit = 200;
        std::string kinds;
        std::string room;        // room_wait: the participant waiting ("" for person_wait)
        std::chrono::steady_clock::time_point deadline;
    };
    std::vector<Waiter> waiters_;
    uint64_t gesturePublished_ = 0;   // the last gesture pushed to subscribers
    void serveGestures();

    // Connection tracking for status indicator
    std::chrono::steady_clock::time_point last_command_time_{};
    int total_commands_ = 0;

    // Wakes the TV event loop when a client connects (see start()).
    std::thread wakeThread_;
    std::atomic<bool> stopWake_{false};

    bool pollOne();
    std::string dispatch(const std::string& line, std::string& cmd);

    // Auth helpers
    bool auth_required() const { return !auth_secret_.empty(); }
    std::string generate_nonce();
    std::string compute_hmac(const std::string& nonce);
    bool authenticate_connection(int fd);
};
