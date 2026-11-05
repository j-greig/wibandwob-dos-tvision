#include "tr808_audio.h"

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace tr808 {

static bool isFile(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string samplesDir() {
    static const std::string dir = [] {
        std::vector<std::string> cands;
        if (const char* e = std::getenv("WWDOS_TR808_SAMPLES")) if (*e) cands.push_back(e);
        if (const char* r = std::getenv("WWDOS_REPO")) if (*r) cands.push_back(std::string(r) + "/modules/tr808/samples");
        cands.push_back("modules/tr808/samples");
#ifdef WWDOS_SOURCE_DIR
        cands.push_back(std::string(WWDOS_SOURCE_DIR) + "/modules/tr808/samples");
#endif
        for (const auto& c : cands)
            if (isFile(c + "/bd.wav")) return c;
        return cands.empty() ? std::string() : cands.back();
    }();
    return dir;
}

static std::atomic<bool> gEnabled{[] {
    const char* e = std::getenv("WWDOS_TR808_AUDIO");
    return !(e && (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' || e[0] == 'F'));
}()};

bool audioEnabled() { return gEnabled.load(); }
void setAudioEnabled(bool on) { gEnabled.store(on); }

static const char* kAfplay = "/usr/bin/afplay";

bool audioAvailable() {
    return ::access(kAfplay, X_OK) == 0 && isFile(samplesDir() + "/bd.wav");
}

namespace {

struct Hit { std::string path; double vol; };

class Player {
public:
    Player() : th_([this] { run(); }) {}
    ~Player() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            quit_ = true;
        }
        cv_.notify_all();
        if (th_.joinable()) th_.join();
    }
    void push(Hit h) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (q_.size() > 64) return;  // runaway: drop
            q_.push_back(std::move(h));
        }
        cv_.notify_one();
    }

private:
    void reap() {
        for (auto it = pids_.begin(); it != pids_.end();) {
            int st = 0;
            pid_t r = ::waitpid(*it, &st, WNOHANG);
            if (r == *it || (r < 0 && errno == ECHILD)) it = pids_.erase(it);
            else ++it;
        }
    }
    void spawn(const Hit& h) {
        reap();
        if (pids_.size() >= 32) return;  // too many voices ringing: drop the hit
        char vol[32];
        std::snprintf(vol, sizeof(vol), "%.3f", h.vol);
        const char* argv[] = {kAfplay, "-v", vol, h.path.c_str(), nullptr};
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
        pid_t pid = 0;
        if (posix_spawn(&pid, kAfplay, &fa, nullptr, const_cast<char* const*>(argv), environ) == 0)
            pids_.push_back(pid);
        posix_spawn_file_actions_destroy(&fa);
    }
    void run() {
        std::unique_lock<std::mutex> lk(mu_);
        while (!quit_) {
            cv_.wait_for(lk, std::chrono::milliseconds(250), [this] { return quit_ || !q_.empty(); });
            while (!q_.empty() && !quit_) {
                Hit h = std::move(q_.front());
                q_.pop_front();
                lk.unlock();
                spawn(h);
                lk.lock();
            }
            lk.unlock();
            reap();
            lk.lock();
        }
        lk.unlock();
        for (pid_t p : pids_) { ::kill(p, SIGTERM); int st; ::waitpid(p, &st, 0); }
        pids_.clear();
    }

    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Hit> q_;
    bool quit_ = false;
    std::vector<pid_t> pids_;  // touched only by the worker thread
    std::thread th_;
};

Player& player() {
    static Player p;
    return p;
}

}  // namespace

void playVoice(const char* voiceId, double volume) {
    if (!audioEnabled() || !voiceId) return;
    std::string path = samplesDir() + "/" + voiceId + ".wav";
    static std::string lastMissing;
    if (!isFile(path)) {
        if (lastMissing != path) {
            std::fprintf(stderr, "[tr808] missing sample %s\n", path.c_str());
            lastMissing = path;
        }
        return;
    }
    player().push({path, volume});
}

}  // namespace tr808
