#include "fire_rate.hpp"
#include "../game/game.hpp"
#include "../game/offsets.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  FIRE RATE — обнуляет fireDuration активного оружия каждый тик.
//
//  Цепочка: LocalPlayer → WeaponRootController(+0x88) → ActiveWeapon(+0xA0) →
//           WeaponController. WeaponController + OFF_WC_FIRE_DURATION (0x108) —
//           таймер до следующего выстрела (SafeFloat). Запись 0.0f заставляет
//           игру думать, что таймер истёк, и пушка стреляет без задержки.
//
//  Без кеша по id: каждый тик перечитываем активное оружие и пишем 0, когда
//  таймер в «боевом» диапазоне (0.02..2.0s). Раньше кеш «залипал» на первом
//  оружии, а узкий диапазон 0.5s не ловил дробовики/винтовки.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632ULL;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static inline bool valid_addr(uint64_t a) {
        return likely_ptr(a);
    }

    static uint64_t get_local_player() {
        uint64_t pm = get_player_manager();
        if (!likely_ptr(pm)) {
            pm = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
        }
        if (!likely_ptr(pm)) return 0;
        return rpm<uint64_t>(pm + oxorany(OFF_PM_LOCAL_PLAYER));
    }

    static uint64_t get_weapon_controller() {
        uint64_t lp = get_local_player();
        if (!likely_ptr(lp)) return 0;

        uint64_t wrc = rpm<uint64_t>(lp + oxorany(OFF_PLAYER_WEAPON_ROOT));
        if (!likely_ptr(wrc)) return 0;

        uint64_t wc = rpm<uint64_t>(wrc + oxorany(OFF_WRC_ACTIVE_WEAPON));
        if (!likely_ptr(wc)) return 0;
        return wc;
    }

    static inline void safe_write_float(uint64_t addr, float value) {
        // OFF_WC_FIRE_DURATION хранится как SafeFloat (salt @+0, value @+4),
        // но игра всё равно читает «сырое» float-поле на стороне таймера —
        // прямого wpm<float> по 0x108 достаточно (так же сделано в fustknife.cpp).
        wpm<float>(addr, value);
    }
}

void fire_rate::run() {
    if (!cfg::fire_rate::enabled()) return;

    uint64_t wc = get_weapon_controller();
    if (!valid_addr(wc)) return;

    // Таймер зануляем только в «боевом» диапазоне:
    //   - cur <= 0.02  → таймер и так истёк, не пишем;
    //   - cur > 2.0    → перезарядка/длинная пауза, не трогаем.
    float cur = 0.f;
    if (!mem_read(wc + oxorany(OFF_WC_FIRE_DURATION), &cur, sizeof(cur))) return;
    if (cur <= 0.02f || cur > 2.0f) return;

    safe_write_float(wc + oxorany(OFF_WC_FIRE_DURATION), 0.0f);
}
