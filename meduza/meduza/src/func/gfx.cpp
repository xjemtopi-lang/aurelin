#include "gfx.hpp"
#include "../other/memory.hpp"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <stdint.h>
#include <vector>

struct AddrVal32 {
    uint64_t addr;
    uint32_t orig;
};

static std::vector<AddrVal32> bk_texture;
static std::vector<AddrVal32> bk_lowgfx;

// ============================================================
//  process_vm helpers
// ============================================================

static bool pvm_write32(uint64_t addr, uint32_t val) {
    if (proc::pid <= 0) return false;
    struct iovec lv, rv;
    lv.iov_base = &val;
    lv.iov_len  = 4;
    rv.iov_base = reinterpret_cast<void*>(addr);
    rv.iov_len  = 4;
    return process_vm_writev(proc::pid, &lv, 1, &rv, 1, 0) == 4;
}

static bool pvm_read32(uint64_t addr, uint32_t* out) {
    if (proc::pid <= 0) return false;
    struct iovec lv, rv;
    lv.iov_base = out;
    lv.iov_len  = 4;
    rv.iov_base = reinterpret_cast<void*>(addr);
    rv.iov_len  = 4;
    return process_vm_readv(proc::pid, &lv, 1, &rv, 1, 0) == 4;
}

// ============================================================
//  /proc/<pid>/maps — все rw регионы >= 4KB
// ============================================================

struct MemRegion {
    uint64_t start;
    uint64_t end;
};

static void get_rw_regions(std::vector<MemRegion>& out) {
    out.clear();
    if (proc::pid <= 0) return;

    char path[64];
    {
        int n = 0;
        const char* p = "/proc/";
        while (*p) path[n++] = *p++;
        int tm = proc::pid;
        char pds[12]; int pl = 0;
        while (tm > 0) { pds[pl++] = '0' + (tm % 10); tm /= 10; }
        for (int i = pl - 1; i >= 0; i--) path[n++] = pds[i];
        const char* s = "/maps";
        while (*s) path[n++] = *s++;
        path[n] = '\0';
    }

    int fd = sys::openat(AT_FDCWD, path, O_RDONLY);
    if (fd < 0) return;

    char buf[4096];
    char line[256];
    int  line_pos = 0;
    long r;

    while ((r = sys::read(fd, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < r; i++) {
            char c = buf[i];
            if (c == '\n' || line_pos >= 255) {
                line[line_pos] = '\0';
                line_pos = 0;

                uint64_t st = 0, en = 0;
                int j = 0;

                // start
                while (line[j] && line[j] != '-') {
                    char hc = line[j++];
                    uint64_t d = 0;
                    if      (hc >= '0' && hc <= '9') d = (uint64_t)(hc - '0');
                    else if (hc >= 'a' && hc <= 'f') d = (uint64_t)(hc - 'a' + 10);
                    else if (hc >= 'A' && hc <= 'F') d = (uint64_t)(hc - 'A' + 10);
                    st = (st << 4) | d;
                }
                if (line[j] == '-') j++;

                // end
                while (line[j] && line[j] != ' ') {
                    char hc = line[j++];
                    uint64_t d = 0;
                    if      (hc >= '0' && hc <= '9') d = (uint64_t)(hc - '0');
                    else if (hc >= 'a' && hc <= 'f') d = (uint64_t)(hc - 'a' + 10);
                    else if (hc >= 'A' && hc <= 'F') d = (uint64_t)(hc - 'A' + 10);
                    en = (en << 4) | d;
                }
                // j стоит на пробеле перед perms
                if (line[j] == ' ') j++;
                // j теперь на первом символе perms

                if (st == 0 || en <= st) continue;
                if (line[j] != 'r' || line[j+1] != 'w') continue;
                if ((en - st) < 0x1000) continue;

                MemRegion reg;
                reg.start = st;
                reg.end   = en;
                out.push_back(reg);

            } else {
                line[line_pos++] = c;
            }
        }
    }

    sys::close(fd);
}

// ============================================================
//  Scan DWORD по всем rw регионам
// ============================================================

static uint8_t g_scan_buf[65536];

static void scan_dword(uint32_t search_val,
                       int      max_results,
                       std::vector<AddrVal32>& results) {
    results.clear();

    std::vector<MemRegion> regions;
    get_rw_regions(regions);
    if (regions.empty()) return;

    int count = 0;

    for (int ri = 0; ri < (int)regions.size() && count < max_results; ri++) {
        uint64_t addr      = regions[ri].start;
        const uint64_t end = regions[ri].end;

        while (addr + 4 <= end && count < max_results) {
            uint64_t remain = end - addr;
            size_t   chunk  = (remain > sizeof(g_scan_buf))
                              ? sizeof(g_scan_buf) : (size_t)remain;

            struct iovec lv, rv;
            lv.iov_base = g_scan_buf;
            lv.iov_len  = chunk;
            rv.iov_base = reinterpret_cast<void*>(addr);
            rv.iov_len  = chunk;

            long got = process_vm_readv(proc::pid, &lv, 1, &rv, 1, 0);
            if (got < 4) {
                addr = end;
                break;
            }

            long aligned = got & ~3L;
            for (long off = 0; off < aligned; off += 4) {
                uint32_t val;
                __builtin_memcpy(&val, g_scan_buf + off, 4);
                if (val == search_val) {
                    AddrVal32 bk;
                    bk.addr = addr + (uint64_t)off;
                    bk.orig = val;
                    results.push_back(bk);
                    count++;
                    if (count >= max_results) break;
                }
            }

            addr += (uint64_t)got;
        }
    }
}

// ============================================================
//  TEXTURE POTATO
//
//  Lua: searchNumber("1073741890", TYPE_DWORD) -> editAll("1086324736")
//  Прямой DWORD скан, backup + restore.
// ============================================================

static const uint32_t TEXTURE_SEARCH = 1073741890u;  // 0x40000042
static const uint32_t TEXTURE_PATCH  = 1086324736u;  // 0x40C00000 = float 6.0

namespace gfx {

void texture_on() {
    if (!bk_texture.empty()) return;
    if (proc::pid <= 0) return;

    scan_dword(TEXTURE_SEARCH, 6666, bk_texture);

    for (int i = 0; i < (int)bk_texture.size(); i++) {
        pvm_write32(bk_texture[i].addr, TEXTURE_PATCH);
    }
}

void texture_off() {
    if (bk_texture.empty()) return;
    for (int i = 0; i < (int)bk_texture.size(); i++) {
        pvm_write32(bk_texture[i].addr, bk_texture[i].orig);
    }
    bk_texture.clear();
}

// ============================================================
//  LOW GFX
//
//  Lua паттерн разбор:
//    searchNumber("178Q;4575657222473777152Q;1065353216Q::13", TYPE_QWORD)
//    -> ищет: [+0]=178, [+8]=4575657222473777152, [+16]=1065353216
//       ::13 = stride 13 QWORD = 104 байта между первым и последним
//    refineNumber("4575657222473777152Q;1065353216Q::5")
//    -> уточняет: [+0]=4575657222473777152, [+8]=1065353216
//       ::5 = stride 5, но это refine уже найденного — шаг ВНУТРИ кластера
//
//  Итог: нас интересует DWORD == 1065353216 (float 1.0 = 0x3F800000)
//  с проверкой что на offset -8 от него лежит QWORD == 4575657222473777152
//
//  4575657222473777152 = 0x3F8000003F800000 = два float 1.0 подряд
//  1065353216          = 0x3F800000         = float 1.0 (DWORD)
//
//  Паттерн в памяти (байты):
//    offset -8: 00 00 80 3F 00 00 80 3F   <- два float 1.0
//    offset  0: 00 00 80 3F               <- float 1.0 (цель записи)
//
//  Пишем: 1259902591 = 0x4B189FFF ≈ float 9999999.0
// ============================================================

// anchor: два float 1.0 как QWORD
static const uint64_t LG_ANCHOR_Q = 4575657222473777152ULL; // 0x3F8000003F800000
// target DWORD: float 1.0
static const uint32_t LG_TARGET_D = 1065353216u;            // 0x3F800000
// patch DWORD
static const uint32_t LG_PATCH    = 1259902591u;            // 0x4B189FFF

void low_gfx_on() {
    if (!bk_lowgfx.empty()) return;
    if (proc::pid <= 0) return;

    // 1. Сначала сканим DWORD == LG_TARGET_D (float 1.0) — много результатов
    std::vector<AddrVal32> candidates;
    scan_dword(LG_TARGET_D, 10000, candidates);

    if (candidates.empty()) return;

    // 2. Для каждого кандидата проверяем: [addr - 8] == LG_ANCHOR_Q
    //    Это и есть refine из lua
    for (int i = 0; i < (int)candidates.size(); i++) {
        uint64_t addr = candidates[i].addr;

        // Нужно минимум 8 байт перед адресом
        if (addr < 8) continue;

        uint64_t anchor_addr = addr - 8;

        // Читаем QWORD по anchor_addr
        uint32_t lo = 0, hi = 0;
        if (!pvm_read32(anchor_addr,     &lo)) continue;
        if (!pvm_read32(anchor_addr + 4, &hi)) continue;

        uint64_t qval = (uint64_t)lo | ((uint64_t)hi << 32);
        if (qval != LG_ANCHOR_Q) continue;

        // Совпало — пишем патч
        AddrVal32 bk;
        bk.addr = addr;
        bk.orig = candidates[i].orig;
        bk_lowgfx.push_back(bk);

        pvm_write32(addr, LG_PATCH);

        if ((int)bk_lowgfx.size() >= 100) break;
    }
}

void low_gfx_off() {
    if (bk_lowgfx.empty()) return;
    for (int i = 0; i < (int)bk_lowgfx.size(); i++) {
        pvm_write32(bk_lowgfx[i].addr, bk_lowgfx[i].orig);
    }
    bk_lowgfx.clear();
}

} // namespace gfx
