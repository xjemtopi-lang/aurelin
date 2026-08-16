#include "fustknife.hpp"
#include "../game/game.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <unordered_set>
#include <vector>

// Fast Knife — обнуляет таймер удара (WeaponController + 0x120),
// если активное оружие = нож (weapon id = 70 в WeaponParameters + 0x18).
namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer  = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController  = 0x88;   // WeaponRootController
    static constexpr uint64_t kOffWeaponryCurrentWeapon     = 0xA0;   // → WeaponController
    static constexpr uint64_t kOffWeaponWeaponParameters    = 0xA8;   // WeaponController → WeaponParameters
    static constexpr uint64_t kOffWeaponParametersId        = 0x18;   // int id
    static constexpr uint64_t kOffWeaponControllerFireTime  = 0x120;  // float — таймер удара

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    // ids оружий, которые считаются ножом
    static const std::unordered_set<int>& knife_ids() {
        static const std::unordered_set<int> ids = { 70 };
        return ids;
    }
}

void fustknife::run() {
    if (!cfg::fustknife::enabled()) return;

    uint64_t player_manager = get_player_manager();
    if (!player_manager) {
        player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
    }
    if (!likely_ptr(player_manager)) return;

    uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(kOffPlayerManagerLocalPlayer));
    if (!likely_ptr(local_player)) return;

    uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(kOffPlayerWeaponryController));
    if (!likely_ptr(weaponry)) return;

    uint64_t weapon_controller = rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon));
    if (!likely_ptr(weapon_controller)) return;

    uint64_t weapon_parameters = rpm<uint64_t>(weapon_controller + oxorany(kOffWeaponWeaponParameters));
    if (!likely_ptr(weapon_parameters)) return;

    int knifeid = rpm<int>(weapon_parameters + oxorany(kOffWeaponParametersId));
    if (knife_ids().count(knifeid) == 0) return;

    // Зануляем таймер только когда он активен и больше порога анимации.
    // Постоянная запись 0 каждый тик ломает замах: удар не успевает
    // проиграться и нож «не режет». Порог 0.15f даёт анимации проиграться.
    float cur = 0.f;
    if (!mem_read(weapon_controller + oxorany(kOffWeaponControllerFireTime),
                  &cur, sizeof(cur))) return;
    if (cur <= 0.15f) return;

    wpm<float>(weapon_controller + oxorany(kOffWeaponControllerFireTime), 0.0f);
}
