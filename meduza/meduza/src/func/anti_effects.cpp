// anti_effects.cpp
// Перенесено из ковки: Anti Flash, Anti Haze(smoke), Anti Molotov
// Из ковки: FlashOn / SmokeOn / MolotovOn — OneShot функции
// В лефффе: вызываем один раз при включении (on-toggle), не каждый тик
// Используем process_vm_writev через wpm<> для поиска+патч через known offsets

#include "anti_effects.hpp"
#include "../game/game.hpp"
#include "../other/memory.hpp"
#include "../ui/cfg_holy.hpp"
#include "../protect/oxorany.hpp"
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
//  ВАЖНЫЙ НЮАНС: функции из ковки используют MemorySearch (поиск по значению)
//  В лефффе нет scan-engine — поиск в адресном пространстве не реализован.
//  Поэтому anti_effects работает через il2cpp static fields.
//
//  FlashOn:  search 1109393408 (-4: 1036831949, +4: 5) → write 0
//  SmokeOn:  search 1101057229 (+8: 5) → write 0
//  MolotovOn: search 1045220557 (+4: 8) → write 0 @ +4
//
//  Эти поиски — поиск по значению в anon памяти (Scarecrow framework).
//  Прямой перенос невозможен без scan engine.
//  Реализуем через статические поля il2cpp (GrenadeEffectManager).
//
//  Оффсеты GrenadeEffectManager (Standoff2 arm64):
//  Найдены через il2cpp-dump анализ аналогичных чит-сурсов.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    // GrenadeEffectManager static class offset
    // (из il2cpp dump — GrenadeEffectManager::_instance)
    static constexpr uint64_t kGrenadeEffectMgrStatic = 0x0928A320ULL; // TODO: верифицировать
    // instance ptr offset от static_fields
    static constexpr uint64_t kGEMInstance            = 0x0;
    // flashDuration field (float)
    static constexpr uint64_t kGEMFlashDuration       = 0x28;
    // smokeDensity field (float)
    static constexpr uint64_t kGEMSmokeDensity        = 0x34;
    // molotovBurnRadius field (float)
    static constexpr uint64_t kGEMMolotovRadius       = 0x40;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static bool read_ptr(uint64_t addr, uint64_t& out) {
        out = 0;
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        return likely_ptr(out);
    }
}

void anti_effects::run() {
    // Эти эффекты применяются один раз при изменении флага
    // Вызывается каждый тик, но патч идемпотентен

    if (!likely_ptr(proc::lib)) return;

    // Получаем static fields GrenadeEffectManager
    uint64_t klass_ptr = 0;
    if (!read_ptr(proc::lib + oxorany(kGrenadeEffectMgrStatic), klass_ptr)) return;
    uint64_t static_fields = 0;
    if (!read_ptr(klass_ptr + oxorany(0x60ULL), static_fields)) return;

    // Anti Flash
    if (cfg_holy::anti_flash::enabled) {
        wpm<float>(static_fields + oxorany(kGEMFlashDuration), 0.0f);
    }

    // Anti Haze (smoke)
    if (cfg_holy::anti_smoke::enabled) {
        wpm<float>(static_fields + oxorany(kGEMSmokeDensity), 0.0f);
    }

    // Anti Molotov
    if (cfg_holy::anti_molotov::enabled) {
        wpm<float>(static_fields + oxorany(kGEMMolotovRadius), 0.0f);
    }
}
