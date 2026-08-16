#include "inf_shop.hpp"
#include "../game/game.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>
#include <vector>

// Inf Shop — патчит WeaponParameters → Ammunition: MagazineCapacity / Capacity
// и их SafeInt-зеркала. По сути работает поверх той же цепочки, что и inf_ammo,
// но имеет независимый toggle/value (для покупок в магазине).
namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy   = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer    = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController    = 0x88;
    static constexpr uint64_t kOffWeaponryCurrentWeapon       = 0xA0;
    static constexpr uint64_t kOffWeaponWeaponParameters      = 0xA8;
    static constexpr uint64_t kOffWeaponParametersAmmunition  = 0x130;

    // Ammunition fields
    static constexpr uint64_t kOffAmmoMagazineCapacity        = 0x10;
    static constexpr uint64_t kOffAmmoCapacity                = 0x12;
    static constexpr uint64_t kOffAmmoMagazineCapacitySafe    = 0x14;
    static constexpr uint64_t kOffAmmoCapacitySafe            = 0x20;

    struct SafeIntMem {
        int salt;
        int value;
    };

    struct NullableSafeIntMem {
        uint8_t has_value;
        uint8_t pad[3];
        SafeIntMem value;
    };

    static bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static void add_unique(std::vector<uint64_t>& out, uint64_t p) {
        if (!likely_ptr(p)) return;
        for (uint64_t v : out) {
            if (v == p) return;
        }
        out.push_back(p);
    }

    static bool write_nullable_safe_int(uint64_t addr, int value) {
        if (!addr) return false;

        NullableSafeIntMem v{};
        if (!mem_read(addr, &v, sizeof(v))) return false;
        v.has_value = 1;
        v.value.salt = 0;
        v.value.value = value;
        return mem_write(addr, &v, sizeof(v));
    }

    static std::vector<uint64_t> collect_weapon_params() {
        std::vector<uint64_t> out;

        uint64_t player_manager = get_player_manager();
        if (!player_manager) {
            player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
        }
        if (!player_manager) return out;

        uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(kOffPlayerManagerLocalPlayer));
        if (!likely_ptr(local_player)) return out;

        std::vector<uint64_t> weaponry_list;
        add_unique(weaponry_list, rpm<uint64_t>(local_player + oxorany(kOffPlayerWeaponryController)));

        for (uint64_t weaponry : weaponry_list) {
            std::vector<uint64_t> weapon_list;
            add_unique(weapon_list, rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon)));

            for (uint64_t weapon : weapon_list) {
                add_unique(out, rpm<uint64_t>(weapon + oxorany(kOffWeaponWeaponParameters)));
            }
        }

        return out;
    }

    static void patch_ammo(uint64_t ammo, short value) {
        if (!likely_ptr(ammo)) return;

        wpm<short>(ammo + oxorany(kOffAmmoMagazineCapacity), value);
        wpm<short>(ammo + oxorany(kOffAmmoCapacity), value);
        write_nullable_safe_int(ammo + oxorany(kOffAmmoMagazineCapacitySafe), value);
        write_nullable_safe_int(ammo + oxorany(kOffAmmoCapacitySafe), value);
    }
}

void inf_shop::run() {
    if (!cfg::inf_shop::enabled()) return;

    int clamped = std::clamp(cfg::inf_shop::value(), 100, 30000);
    short value = static_cast<short>(clamped);

    auto params_list = collect_weapon_params();
    for (uint64_t params : params_list) {
        uint64_t ammo = rpm<uint64_t>(params + oxorany(kOffWeaponParametersAmmunition));
        if (!likely_ptr(ammo)) continue;
        patch_ammo(ammo, value);
    }
}
