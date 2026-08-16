#include "chams.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// White Chams: scan all rw- regions of target process for SEARCH_VALUE
// and replace with REPLACE_VALUE via process_vm_writev.
// Restores on toggle-off.
// Target: com.axlebolt.standoff2 (arm64-v8a).
// Requires CAP_SYS_PTRACE or same UID (root launch).
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    static constexpr uint32_t SEARCH_VALUE  = 1073741859u;
    static constexpr uint32_t REPLACE_VALUE = 1073741910u;

    struct BackupEntry {
        uint64_t address;
        uint32_t original_value;
    };

    static std::vector<BackupEntry> g_backup;
    static std::mutex               g_backup_mutex;

    static std::atomic<bool> g_worker_active{false};
    static std::thread       g_worker_thread;
    static int               g_last_pid = -1;  // для детекта рестарта процесса

    // ── Find target PID by cmdline ───────────────────────────────────────────
    static int find_target_pid() {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;
        struct dirent* entry;
        char path[64];
        char cmdline[256];
        while ((entry = readdir(dir)) != nullptr) {
            int id = atoi(entry->d_name);
            if (id <= 0) continue;
            std::snprintf(path, sizeof(path), "/proc/%d/cmdline", id);
            FILE* fp = std::fopen(path, "r");
            if (!fp) continue;
            std::memset(cmdline, 0, sizeof(cmdline));
            std::fread(cmdline, 1, sizeof(cmdline) - 1, fp);
            std::fclose(fp);
            if (std::strstr(cmdline, "com.axlebolt.standoff2") != nullptr) {
                closedir(dir);
                return id;
            }
        }
        closedir(dir);
        return -1;
    }

    // ── Scan & patch ─────────────────────────────────────────────────────────
    static void apply_patch(int pid) {
        char maps_path[64];
        std::snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
        FILE* maps = std::fopen(maps_path, "r");
        if (!maps) return;

        const size_t CHUNK = 4u * 1024u * 1024u;
        uint8_t* buf = static_cast<uint8_t*>(std::malloc(CHUNK));
        if (!buf) { std::fclose(maps); return; }

        std::vector<BackupEntry> local_backup;
        char line[512];

        while (std::fgets(line, sizeof(line), maps) != nullptr) {
            // Drain remainder of an over-long line
            if (std::strchr(line, '\n') == nullptr) {
                int c;
                while ((c = std::fgetc(maps)) != '\n' && c != EOF) { /* skip */ }
            }

            unsigned long long rs_ull = 0, re_ull = 0;
            char perms[8] = {0};
            if (std::sscanf(line, "%llx-%llx %7s", &rs_ull, &re_ull, perms) < 3) continue;

            uint64_t rs = static_cast<uint64_t>(rs_ull);
            uint64_t re = static_cast<uint64_t>(re_ull);
            if (re <= rs) continue;

            // Need read+write
            if (!std::strchr(perms, 'r') || !std::strchr(perms, 'w')) continue;

            uint64_t region_size = re - rs;
            if (region_size < sizeof(uint32_t)) continue;

            // Snapshot existing addresses to avoid O(N*M) scan inside hot loop
            // (single-threaded worker => safe to read g_backup without lock)
            const size_t existing_count = g_backup.size();

            for (uint64_t off = 0; off < region_size; off += CHUNK) {
                size_t to_read = static_cast<size_t>(region_size - off);
                if (to_read > CHUNK) to_read = CHUNK;
                if (to_read < sizeof(uint32_t)) break;

                struct iovec local_iov;
                local_iov.iov_base = buf;
                local_iov.iov_len  = to_read;

                struct iovec remote_iov;
                remote_iov.iov_base = reinterpret_cast<void*>(rs + off);
                remote_iov.iov_len  = to_read;

                ssize_t rd = syscall(SYS_process_vm_readv, pid, &local_iov, 1, &remote_iov, 1, 0);
                if (rd != static_cast<ssize_t>(to_read)) continue;

                for (size_t i = 0; i + sizeof(uint32_t) <= to_read; i += sizeof(uint32_t)) {
                    uint32_t value;
                    std::memcpy(&value, buf + i, sizeof(uint32_t));
                    if (value != SEARCH_VALUE) continue;

                    uint64_t addr = rs + off + i;

                    // De-dup against already backed-up addresses
                    bool exists = false;
                    for (size_t k = 0; k < existing_count; ++k) {
                        if (g_backup[k].address == addr) { exists = true; break; }
                    }
                    if (!exists) {
                        BackupEntry be;
                        be.address        = addr;
                        be.original_value = value;
                        local_backup.push_back(be);
                    }

                    uint32_t new_val = REPLACE_VALUE;
                    struct iovec wlocal;
                    wlocal.iov_base = &new_val;
                    wlocal.iov_len  = sizeof(uint32_t);
                    struct iovec wremote;
                    wremote.iov_base = reinterpret_cast<void*>(addr);
                    wremote.iov_len  = sizeof(uint32_t);
                    syscall(SYS_process_vm_writev, pid, &wlocal, 1, &wremote, 1, 0);
                }
            }
        }

        std::fclose(maps);
        std::free(buf);

        if (!local_backup.empty()) {
            std::lock_guard<std::mutex> lock(g_backup_mutex);
            g_backup.insert(g_backup.end(), local_backup.begin(), local_backup.end());
        }
    }

    // ── Restore originals ────────────────────────────────────────────────────
    static void restore_backup(int pid) {
        std::lock_guard<std::mutex> lock(g_backup_mutex);
        for (size_t i = 0; i < g_backup.size(); ++i) {
            uint32_t orig = g_backup[i].original_value;
            struct iovec wlocal;
            wlocal.iov_base = &orig;
            wlocal.iov_len  = sizeof(uint32_t);
            struct iovec wremote;
            wremote.iov_base = reinterpret_cast<void*>(g_backup[i].address);
            wremote.iov_len  = sizeof(uint32_t);
            syscall(SYS_process_vm_writev, pid, &wlocal, 1, &wremote, 1, 0);
        }
        g_backup.clear();
    }

    // ── Worker loop ──────────────────────────────────────────────────────────
    static void worker_loop() {
        while (g_worker_active.load(std::memory_order_acquire)) {
            int pid = find_target_pid();
            if (pid > 0) {
                // Если PID сменился — игра перезапущена, старые адреса невалидны.
                if (g_last_pid != -1 && g_last_pid != pid) {
                    std::lock_guard<std::mutex> lock(g_backup_mutex);
                    g_backup.clear();
                }
                g_last_pid = pid;

                if (cfg::chams::enabled()) {
                    apply_patch(pid);
                } else {
                    restore_backup(pid);
                }
            }
            // Sleep ~5s, but stay responsive to stop flag (50 * 100ms)
            for (int i = 0; i < 50; ++i) {
                if (!g_worker_active.load(std::memory_order_acquire)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        // Final restore on shutdown
        int pid = find_target_pid();
        if (pid > 0) restore_backup(pid);
    }

    static void start_worker() {
        bool expected = false;
        if (!g_worker_active.compare_exchange_strong(expected, true)) return;
        // Safety: if previous thread object is somehow still joinable (shouldn't be)
        if (g_worker_thread.joinable()) g_worker_thread.join();
        g_worker_thread = std::thread(worker_loop);
    }

    static void stop_worker() {
        bool expected = true;
        if (!g_worker_active.compare_exchange_strong(expected, false)) return;
        if (g_worker_thread.joinable()) g_worker_thread.join();
        std::lock_guard<std::mutex> lock(g_backup_mutex);
        g_backup.clear();
        g_last_pid = -1;
    }
} // namespace

void chams::run() {
    if (cfg::chams::enabled()) {
        start_worker();
    } else {
        stop_worker();
    }
}
