#pragma once

// Fire Rate — обнуляет таймер выстрела (WeaponController + OFF_WC_FIRE_DURATION),
// если активное оружие — пушка (weapon id 1..69).
namespace fire_rate {
    void run();
}
