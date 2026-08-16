#include "wallshot.hpp"
#include "../game/game.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>
#include <vector>

namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController = 0x88;
    static constexpr uint64_t kOffWeaponryCurrentWeapon = 0xA0;
    static constexpr uint64_t kOffWeaponWeaponParameters = 0xA8;
    static constexpr uint64_t kOffWeaponParametersAmmunition = 0x130;

    // GunParameters
    static constexpr uint64_t kOffGunParametersPenetrationPower = 0x1A4;
    static constexpr uint64_t kOffGunParametersPenetrationPowerSafe = 0x264;

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

    static bool valid_ammo_object(uint64_t params) {
        uint64_t ammo = rpm<uint64_t>(params + oxorany(kOffWeaponParametersAmmunition));
        if (!likely_ptr(ammo)) return false;

        short mag = 0;
        short cap = 0;
        if (!mem_read(ammo + oxorany(0x10), &mag, sizeof(mag))) return false;
        if (!mem_read(ammo + oxorany(0x12), &cap, sizeof(cap))) return false;
        return mag >= 0 && mag <= 30000 && cap >= 0 && cap <= 30000;
    }

    static bool write_nullable_safe_int(uint64_t addr, int value) {
        if (!likely_ptr(addr)) return false;

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
        if (!likely_ptr(player_manager)) return out;

        uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(kOffPlayerManagerLocalPlayer));
        if (!likely_ptr(local_player)) return out;

        std::vector<uint64_t> weaponry_list;
        add_unique(weaponry_list, rpm<uint64_t>(local_player + oxorany(kOffPlayerWeaponryController)));

        for (uint64_t weaponry : weaponry_list) {
            uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon));
            if (!likely_ptr(weapon)) continue;
            uint64_t params = rpm<uint64_t>(weapon + oxorany(kOffWeaponWeaponParameters));
            if (!likely_ptr(params)) continue;
            if (!valid_ammo_object(params)) continue;
            add_unique(out, params);
        }

        return out;
    }

    static void write_wallshot_value(int value) {
        auto params_list = collect_weapon_params();
        for (uint64_t params : params_list) {
            wpm<int>(params + oxorany(kOffGunParametersPenetrationPower), value);
            write_nullable_safe_int(params + oxorany(kOffGunParametersPenetrationPowerSafe), value);
        }
    }
}

void wallshot::run() {
    static bool prev_enabled = false;
    bool enabled = cfg::wallshot::enabled();

    if (enabled) {
        int value = std::clamp(cfg::wallshot::value(), 150, 2147483646);
        write_wallshot_value(value);
    } else if (prev_enabled) {
        write_wallshot_value(cfg::wallshot::restore_value());
    }

    prev_enabled = enabled;
}
