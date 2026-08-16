#include "sigma.hpp"
#include "../game/game.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  SIGMA — ONE HIT KILL
//  Логика взята из исходного снипа:
//      if (weapon.onehitkill) {
//          if (weaponids.count(weaponid) == 0) return;
//          uintptr_t damage = rpm<uintptr_t>(weaponparameters + 0x140);
//          wpm<int>(damage + 0x28 + 4, 130);
//          wpm<int>(damage + 0x34 + 4, 130);
//          wpm<int>(damage + 0x40 + 4, 130);
//          wpm<int>(damage + 0x4C + 4, 130);
//      }
//  Здесь это интегрировано в наш фреймворк (cfg::sigma + safe-чтения).
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    // ── Цепочка указателей (как в norecoil/inf_ammo/fustknife) ───────────────
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer  = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController  = 0x88;   // WeaponRootController
    static constexpr uint64_t kOffWeaponryCurrentWeapon     = 0xA0;   // → WeaponController
    static constexpr uint64_t kOffWeaponWeaponParameters    = 0xA8;   // WeaponController → WeaponParameters
    static constexpr uint64_t kOffWeaponParametersId        = 0x18;   // int weapon id
    static constexpr uint64_t kOffWeaponParametersAmmunition = 0x130; // для проверки "это ствол"
    static constexpr uint64_t kOffWeaponParametersDamage    = 0x140;  // → DamageData

    // Смещения зон поражения внутри DamageData.
    // +4 к каждой зоне = поле value в SafeIntMem { int salt; int value; }.
    static constexpr uint64_t kOffDamageZone1 = 0x28; // голова
    static constexpr uint64_t kOffDamageZone2 = 0x34; // торс
    static constexpr uint64_t kOffDamageZone3 = 0x40; // руки
    static constexpr uint64_t kOffDamageZone4 = 0x4C; // ноги
    static constexpr uint64_t kSafeIntValueOff = 0x4; // skip salt → write value

    // ID ножа (Fast Knife уже использует 70). Для Sigma исключаем нож.
    static constexpr int kKnifeId = 70;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    // Проверка "это ствол, а не нож/граната/предмет" по полю Ammunition
    static bool weapon_is_probably_gun(uint64_t weapon_params) {
        if (!likely_ptr(weapon_params)) return false;
        uint64_t ammo = rpm<uint64_t>(weapon_params + oxorany(kOffWeaponParametersAmmunition));
        if (!likely_ptr(ammo)) return false;
        short mag = 0, cap = 0;
        if (!mem_read(ammo + oxorany(0x10), &mag, sizeof(mag))) return false;
        if (!mem_read(ammo + oxorany(0x12), &cap, sizeof(cap))) return false;
        return mag >= 0 && mag <= 30000 && cap >= 0 && cap <= 30000;
    }

    // Whitelist weapon ids — собирается на лету (любой валидный ствол, кроме ножа).
    // Это эквивалент `weaponids.count(weaponid)` из исходного снипа, но
    // вместо хардкода списка мы проверяем "оружие выглядит как ствол".
    static bool weaponid_allowed(int weapon_id, uint64_t weapon_params) {
        if (weapon_id == kKnifeId) return false;
        return weapon_is_probably_gun(weapon_params);
    }
}

void sigma::run() {
    if (!cfg::sigma::enabled()) return;

    // 1) Достаём PlayerManager
    uint64_t player_manager = get_player_manager();
    if (!player_manager) {
        player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
    }
    if (!likely_ptr(player_manager)) return;

    // 2) LocalPlayer
    uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(kOffPlayerManagerLocalPlayer));
    if (!likely_ptr(local_player)) return;

    // 3) WeaponRootController
    uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(kOffPlayerWeaponryController));
    if (!likely_ptr(weaponry)) return;

    // 4) Текущий WeaponController
    uint64_t weapon_controller = rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon));
    if (!likely_ptr(weapon_controller)) return;

    // 5) WeaponParameters
    uint64_t weapon_parameters = rpm<uint64_t>(weapon_controller + oxorany(kOffWeaponWeaponParameters));
    if (!likely_ptr(weapon_parameters)) return;

    // 6) Проверка weapon id (как в исходном снипе: weaponids.count(weaponid))
    int weapon_id = rpm<int>(weapon_parameters + oxorany(kOffWeaponParametersId));
    if (!weaponid_allowed(weapon_id, weapon_parameters)) return;

    // 7) Указатель на DamageData (WeaponParameters + 0x140)
    uint64_t damage = rpm<uint64_t>(weapon_parameters + oxorany(kOffWeaponParametersDamage));
    if (!likely_ptr(damage)) return;

    // 8) Значение урона (по умолчанию 130 как в исходном снипе)
    int dmg = std::clamp(cfg::sigma::damage(), 1, 1000);

    // 9) Патчим 4 зоны (+4 → пропуск salt-поля в SafeIntMem)
    wpm<int>(damage + oxorany(kOffDamageZone1 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone2 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone3 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone4 + kSafeIntValueOff), dmg);
}
