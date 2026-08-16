// movement.cpp
// Перенесено из ковки: Air Jump, Strafe, Bunny Hop
// Адаптировано под memory.hpp лефффа (rpm/wpm/process_vm_readv)
// Оффсеты взяты из offsets.hpp лефффа

#include "movement.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../other/memory.hpp"
#include "../ui/cfg_holy.hpp"
#include "../protect/oxorany.hpp"
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Внутренние константы
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    // Player → MovementController
    static constexpr uint64_t kOffMovCtrl       = 0x98;
    // MovementController → TrajectoryPredictor
    static constexpr uint64_t kOffTrajPredictor = 0xA8;
    // TrajectoryPredictor → JumpParams
    static constexpr uint64_t kOffJumpParams    = 0x50;
    // JumpParams → jumpSpeed (float) — Air Jump & Bunny Hop
    static constexpr uint64_t kOffJumpSpeed     = 0x10;
    // JumpParams → jumpSpeed2 (float) — Bunny Hop airstrafe
    static constexpr uint64_t kOffJumpSpeed2    = 0x60;
    // MovementController → ThrustData
    static constexpr uint64_t kOffThrustData    = 0xB0;
    // ThrustData → thrustVec (vec3) — Strafe
    static constexpr uint64_t kOffThrustVec     = 0x68;

    // Значения из ковки (BunnyOn: search 1053609165, write 1081674666)
    // Перевод float-bit значений:
    // 1053609165 (0x3EC00005) = ~0.375f  — нормальный jumpSpeed
    // 1081674666 (0x407A99AA) = ~3.914f  — bunnyhop jumpSpeed
    // 1084227584 (0x409C0000) = ~4.875f  — offset check (не пишем, только читаем)
    // 1112014848 (0x424C0000) = ~51.0f   — offset check
    // Air: 1071225242, 1051260355; freeze val = 5 (DWORD @ +52)
    // Strafe: 1082130432 = 4.0f, write 1232348144 = 2.6e11f (огромное)

    // Нормальное значение jumpSpeed для restore
    static constexpr float kJumpSpeedNormal   = 0.375f;
    // Bunny hop: увеличиваем jumpSpeed
    static constexpr float kJumpSpeedBunny    = 3.914f;
    // Air jump: freeze jumpSpeed на 5 (как в ковке: DWORD=5 ~ 7e-45f float)
    // В ковке пишут int 5 как DWORD — это float 7.0e-45f, но реально это
    // скорее всего int поле. Пишем uint32_t 5.
    static constexpr uint32_t kAirJumpVal     = 5u;
    // Restore air jump: write 30 (DWORD)
    static constexpr uint32_t kAirJumpRestore = 30u;
    // Strafe thrust factor
    static constexpr float kStrafeThrustX     = 0.0f;
    static constexpr float kStrafeThrustZ     = 0.0f;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static inline bool read_ptr(uint64_t addr, uint64_t& out) {
        out = 0;
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        return likely_ptr(out);
    }

    // Получаем MovementController локального игрока
    static uint64_t get_mov_ctrl() {
        uint64_t pm = get_player_manager();
        if (!likely_ptr(pm)) return 0;
        uint64_t lp = 0;
        if (!read_ptr(pm + oxorany(0x70ULL), lp)) return 0;
        uint64_t mc = 0;
        if (!read_ptr(lp + oxorany(kOffMovCtrl), mc)) return 0;
        return mc;
    }

    // Получаем Translation/Trajectory параметры из MovementController
    static uint64_t get_translation_params(uint64_t mc) {
        uint64_t tp = 0;
        if (!read_ptr(mc + oxorany(kOffTrajPredictor), tp)) return 0;
        return tp;
    }

    // Получаем JumpParams из MovementController
    static uint64_t get_jump_params(uint64_t mc) {
        uint64_t tp = get_translation_params(mc);
        if (!tp) return 0;
        uint64_t jp = 0;
        if (!read_ptr(tp + oxorany(kOffJumpParams), jp)) return 0;
        return jp;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  AIR JUMP
//  Из ковки: AirOn() → MemoryFreeze_DWORD(5, Res, 52)
//  Пишем uint32_t 5 в JumpParams+kOffJumpSpeed каждый тик пока включено
// ─────────────────────────────────────────────────────────────────────────────
void air_jump::run() {
    if (!cfg_holy::air_jump::enabled) return;

    uint64_t mc = get_mov_ctrl();
    if (!mc) return;
    uint64_t jp = get_jump_params(mc);
    if (!jp) return;

    // Пишем как uint32 (DWORD) = 5, аналог MemoryFreeze_DWORD(5, Res, 52)
    wpm<uint32_t>(jp + oxorany(kOffJumpSpeed), kAirJumpVal);
}

// ─────────────────────────────────────────────────────────────────────────────
//  STRAFE (авто-стрейф)
//  В воздухе каждый тик толкаем игрока вперёд по направлению взгляда
//  (горизонтальный thrust в TransformData→thrustVec). На земле не трогаем —
//  бег не ломается. Разовый патч из ковки (4.0f → 256006) в нашем
//  расположении не «ощущался», поэтому делаем динамический толчок.
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    static bool  s_strafe_active = false;
    static float s_prev_y       = 0.f;
    static bool  s_prev_valid   = false;

    // TransformData → position vec3 (x@0x44, y@0x48, z@0x4C)
    static bool read_pos(uint64_t td, float& px, float& py, float& pz) {
        if (!mem_read(td + oxorany(0x44ULL), &px, sizeof(px))) return false;
        if (!mem_read(td + oxorany(0x48ULL), &py, sizeof(py))) return false;
        if (!mem_read(td + oxorany(0x4CULL), &pz, sizeof(pz))) return false;
        return true;
    }
}

void strafe::run() {
    const bool want = cfg_holy::strafe::enabled;
    if (!want) {
        s_strafe_active = false;
        s_prev_valid    = false;
        return;
    }
    if (!s_strafe_active) {
        s_strafe_active = true;
        s_prev_valid    = false;
    }

    uint64_t mc = get_mov_ctrl();
    if (!mc) return;
    uint64_t td = 0;
    if (!read_ptr(mc + oxorany(kOffThrustData), td)) return;

    float px = 0.f, py = 0.f, pz = 0.f;
    if (!read_pos(td, px, py, pz)) return;

    // В воздухе? Вертикальная скорость заметно меняет y между тиками.
    bool airborne = s_prev_valid && fabsf(py - s_prev_y) > 0.004f;
    s_prev_y      = py;
    s_prev_valid  = true;
    if (!airborne) return;

    // Направление взгляда локального игрока (строка 3 матрицы, XZ)
    uint64_t pm = get_player_manager();
    if (!likely_ptr(pm)) return;
    uint64_t lp = 0;
    if (!read_ptr(pm + oxorany(0x70ULL), lp)) return;

    matrix vm = player::view_matrix(lp);
    float fx = vm.m31, fz = vm.m33;
    float fl = sqrtf(fx * fx + fz * fz);
    if (fl < 0.001f) return;
    fx /= fl;
    fz /= fl;

    // Умеренный толчок: в ~3 раза сильнее скорости бега, чтобы разгон в воздухе
    // был заметен, но не «швыряло» по карте.
    const float k = 18.f;
    float vec[3] = { fx * k, 0.f, fz * k };
    mem_write(td + oxorany(kOffThrustVec), vec, sizeof(vec));
}

// ─────────────────────────────────────────────────────────────────────────────
//  BUNNY HOP
//  Из ковки: BunnyOn() → search 1053609165 (+32=1084227584, +64=1112014848)
//  write 1081674666 @ +0
//  BunnyOff() → search 1081674666, write 1053609165
//  В нашей реализации: пишем float в jumpSpeed и jumpSpeed2
// ─────────────────────────────────────────────────────────────────────────────
void bunny_hop::run() {
    if (!cfg_holy::bunny_hop::enabled) return;

    uint64_t mc = get_mov_ctrl();
    if (!mc) return;
    uint64_t jp = get_jump_params(mc);
    if (!jp) return;

    // BunnyOn: write 1081674666 as DWORD (= ~3.914f)
    wpm<uint32_t>(jp + oxorany(kOffJumpSpeed),  1081674666u);
    wpm<uint32_t>(jp + oxorany(kOffJumpSpeed2), 1081674666u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CROUCH SPEED
//  Из Wintex: TranslationParameters + 0x48 -> crouch params, далее три float
//  на +0x10 / +0x14 / +0x20 = 990.0f
// ─────────────────────────────────────────────────────────────────────────────
void crouch_speed::run() {
    if (!cfg_holy::crouch_speed::enabled) return;

    uint64_t mc = get_mov_ctrl();
    if (!mc) return;
    uint64_t tp = get_translation_params(mc);
    if (!tp) return;

    uint64_t crouch_params = 0;
    if (!read_ptr(tp + oxorany(0x48ULL), crouch_params)) return;

    wpm<float>(crouch_params + oxorany(0x10ULL), 990.0f);
    wpm<float>(crouch_params + oxorany(0x14ULL), 990.0f);
    wpm<float>(crouch_params + oxorany(0x20ULL), 990.0f);
}
