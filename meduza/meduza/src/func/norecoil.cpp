#include "norecoil.hpp"
#include "../game/game.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>
#include <cmath>

namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController = 0x88;
    static constexpr uint64_t kOffWeaponryCurrentWeapon = 0xA0;
    static constexpr uint64_t kOffWeaponWeaponParameters = 0xA8;
    static constexpr uint64_t kOffWeaponParametersAmmunition = 0x130;

    static constexpr uint64_t kOffGunAccuracyData = 0x228;
    static constexpr uint64_t kOffGunRecoilControl = 0x160;
    static constexpr uint64_t kOffGunSafeRadius = 0x1EC;
    static constexpr uint64_t kOffGunSafeMinRandom = 0x1F4;
    static constexpr uint64_t kOffGunSafeMaxRandom = 0x1FC;
    static constexpr uint64_t kOffGunSafeExtra = 0x204;

    static constexpr uint64_t kOffGunParametersRecoilControl = 0x150;
    static constexpr uint64_t kOffGunParametersRecoilParameters = 0x158;
    static constexpr uint64_t kOffGunParametersRecoilMultOnCrouch = 0x178;
    static constexpr uint64_t kOffGunParametersRecoilAimMult = 0x180;
    static constexpr uint64_t kOffGunParametersRecoilAimMultOnCrouch = 0x188;
    static constexpr uint64_t kOffGunParametersRecoilMultOnCrouchSafe = 0x1F8;
    static constexpr uint64_t kOffGunParametersRecoilAimMultSafe = 0x210;
    static constexpr uint64_t kOffGunParametersRecoilAimMultOnCrouchSafe = 0x228;
    static constexpr uint64_t kOffGunParametersRecoilControlSafe = 0x240;

    struct SafeFloatInline {
        int salt;
        float value;
    };

    static bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static bool read_ptr(uint64_t addr, uint64_t& out) {
        out = 0;
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        if (!likely_ptr(out)) return false;
        return true;
    }

    static bool disable_nullable_safe(uint64_t addr) {
        if (!likely_ptr(addr)) return false;
        return wpm<uint8_t>(addr, static_cast<uint8_t>(0));
    }

    static bool read_safe_float_inline(uint64_t addr, SafeFloatInline& out) {
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        if (!std::isfinite(out.value)) return false;
        if (out.value < -1000.f || out.value > 1000.f) return false;
        return true;
    }

    static bool write_safe_float_inline_value(uint64_t addr, float value) {
        SafeFloatInline s{};
        if (!read_safe_float_inline(addr, s)) return false;
        s.value = value;
        return mem_write(addr, &s, sizeof(s));
    }

    static uint64_t get_local_player() {
        uint64_t player_manager = get_player_manager();
        if (!player_manager) {
            player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
        }
        if (!likely_ptr(player_manager)) return 0;

        uint64_t local = 0;
        if (!read_ptr(player_manager + oxorany(kOffPlayerManagerLocalPlayer), local)) return 0;
        return local;
    }

    static uint64_t get_current_weapon(uint64_t local) {
        if (!likely_ptr(local)) return 0;

        uint64_t weaponry = 0;
        if (!read_ptr(local + oxorany(kOffPlayerWeaponryController), weaponry)) return 0;

        uint64_t weapon = 0;
        if (!read_ptr(weaponry + oxorany(kOffWeaponryCurrentWeapon), weapon)) return 0;
        return weapon;
    }

    static bool weapon_is_probably_gun(uint64_t weapon) {
        if (!likely_ptr(weapon)) return false;

        uint64_t params = 0;
        if (!read_ptr(weapon + oxorany(kOffWeaponWeaponParameters), params)) return false;

        uint64_t ammo = 0;
        if (!read_ptr(params + oxorany(kOffWeaponParametersAmmunition), ammo)) return false;

        short mag = 0;
        short cap = 0;
        if (!mem_read(ammo + oxorany(0x10), &mag, sizeof(mag))) return false;
        if (!mem_read(ammo + oxorany(0x12), &cap, sizeof(cap))) return false;
        return mag >= 0 && mag <= 30000 && cap >= 0 && cap <= 30000;
    }

    static bool patch_accuracy(uint64_t weapon) {
        uint64_t accuracy = 0;
        if (!read_ptr(weapon + oxorany(kOffGunAccuracyData), accuracy)) return false;

        float a = 0.f;
        float b = 0.f;
        if (!mem_read(accuracy + oxorany(0x10), &a, sizeof(a))) return false;
        if (!mem_read(accuracy + oxorany(0x14), &b, sizeof(b))) return false;
        if (!std::isfinite(a) || !std::isfinite(b)) return false;

        bool ok = true;
        ok &= wpm<float>(accuracy + oxorany(0x10), 0.f);
        ok &= wpm<float>(accuracy + oxorany(0x14), 0.f);
        ok &= wpm<float>(accuracy + oxorany(0x18), 0.f);
        return ok;
    }

    static bool patch_recoil_control(uint64_t weapon, float mult) {
        uint64_t recoil_control = 0;
        if (!read_ptr(weapon + oxorany(kOffGunRecoilControl), recoil_control)) return false;

        bool ok = false;
        ok |= write_safe_float_inline_value(recoil_control + oxorany(0x50), mult);
        ok |= write_safe_float_inline_value(recoil_control + oxorany(0x58), 0.f);
        ok |= write_safe_float_inline_value(recoil_control + oxorany(0x60), 0.f);
        return ok;
    }

    static bool patch_recoil_params(uint64_t params) {
        uint64_t recoil_params = 0;
        if (!read_ptr(params + oxorany(kOffGunParametersRecoilParameters), recoil_params)) return false;

        bool ok = true;
        ok &= wpm<float>(recoil_params + oxorany(0x10), 0.f); // horizontalRange
        ok &= wpm<float>(recoil_params + oxorany(0x14), 0.f); // verticalRange
        ok &= wpm<float>(recoil_params + oxorany(0x48), 0.f); // cameraDeviationCoeff
        ok &= wpm<float>(recoil_params + oxorany(0x4C), 0.f); // maxApproachSpeed
        return ok;
    }

    static bool patch_gun_parameters(uint64_t params, float mult) {
        if (!likely_ptr(params)) return false;

        bool ok = false;
        ok |= wpm<int>(params + oxorany(kOffGunParametersRecoilControl), 10000);
        ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilMultOnCrouch), mult);
        ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilAimMult), mult);
        ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilAimMultOnCrouch), mult);

        ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilMultOnCrouchSafe));
        ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilAimMultSafe));
        ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilAimMultOnCrouchSafe));
        ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilControlSafe));

        ok |= patch_recoil_params(params);
        return ok;
    }
}

void norecoil::run() {
    if (!cfg::norecoil::enabled()) return;

    uint64_t local = get_local_player();
    if (!local) return;

    uint64_t weapon = get_current_weapon(local);
    if (!weapon) return;
    if (!weapon_is_probably_gun(weapon)) return;

    uint64_t params = 0;
    if (!read_ptr(weapon + oxorany(kOffWeaponWeaponParameters), params)) return;

    float mult = std::clamp(cfg::norecoil::multiplier(), 0.0f, 0.50f);

    patch_accuracy(weapon);
    patch_recoil_control(weapon, mult);
    patch_gun_parameters(params, mult);
    write_safe_float_inline_value(weapon + oxorany(kOffGunSafeRadius), mult);
    write_safe_float_inline_value(weapon + oxorany(kOffGunSafeMinRandom), 0.f);
    write_safe_float_inline_value(weapon + oxorany(kOffGunSafeMaxRandom), 0.f);
    write_safe_float_inline_value(weapon + oxorany(kOffGunSafeExtra), 0.f);
}
