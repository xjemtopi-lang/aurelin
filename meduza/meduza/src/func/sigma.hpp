#pragma once

// Sigma — One Hit Kill
// Патчит структуру damage в WeaponParameters + 0x140:
// записывает damage value в 4 зоны (голова/торс/руки/ноги).
namespace sigma {
    void run();
}
