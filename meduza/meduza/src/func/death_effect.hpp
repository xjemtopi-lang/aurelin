#pragma once

// ================================================================
// death_effect.hpp  —  Death Particle System
// Стек: C++14, NDK r17, arm64-v8a, ImGui, без STL map/unordered_map
// Принцип: статический пул, никакого new/delete, header-only
// ================================================================

#include "imgui.h"
#include <cmath>
#include <cstring>
#include <stdint.h>

// ----------------------------------------------------------------
// Внешние зависимости из проекта
// ----------------------------------------------------------------
extern float g_sw;
extern float g_sh;

namespace death_effect {

// ================================================================
// КОНСТАНТЫ
// ================================================================
static const int   MAX_PARTICLES    = 512;  // максимум частиц в пуле
static const int   MAX_DEATH_TRACKS = 64;   // максимум отслеживаемых ptr
static const int   PARTICLES_PER_DEATH = 28; // частиц на одну смерть
static const float PARTICLE_LIFETIME = 1.4f; // секунд жизни частицы
static const float GRAVITY           = 180.f; // px/s^2 притяжение вниз
static const float RADIUS_MIN        = 2.5f;
static const float RADIUS_MAX        = 5.5f;

// ================================================================
// СТРУКТУРЫ
// ================================================================

struct Particle {
    float x, y;        // текущая позиция (экранная)
    float vx, vy;      // скорость px/s
    float radius;      // радиус круга
    float born;        // ImGui::GetTime() при рождении
    float lifetime;    // индивидуальное время жизни (небольшой разброс)
    ImU32 color;       // цвет (задаётся при спавне)
    bool  alive;       // слот занят
};

// Запись для трекера смертей
struct DeathTrack {
    uint64_t ptr;       // указатель на игрока
    int      last_hp;   // HP на прошлом кадре
    bool     used;      // слот занят
};

// ================================================================
// ПУЛЫ (статика — без heap allocation)
// ================================================================
static Particle    g_pool[MAX_PARTICLES];
static DeathTrack  g_tracks[MAX_DEATH_TRACKS];
static bool        g_initialized = false;

// ================================================================
// ИНИЦИАЛИЗАЦИЯ
// ================================================================
inline void init() {
    if (g_initialized) return;
    memset(g_pool,   0, sizeof(g_pool));
    memset(g_tracks, 0, sizeof(g_tracks));
    g_initialized = true;
}

// ================================================================
// ПРОСТОЙ LCG ПСЕВДОРАНДОМ
// (без std::rand — он под мьютексом в bionic, медленно)
// ================================================================
static uint32_t g_rng_state = 0xDEADBEEFu;

inline uint32_t rng_next() {
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 17;
    g_rng_state ^= g_rng_state << 5;
    return g_rng_state;
}

// [-1.0, 1.0]
inline float rng_f() {
    return (float)(int32_t)rng_next() / (float)0x7FFFFFFF;
}

// [lo, hi]
inline float rng_range(float lo, float hi) {
    uint32_t r = rng_next();
    float t = (float)(r & 0xFFFF) / 65535.f;
    return lo + t * (hi - lo);
}

// ================================================================
// SPAWN — рождаем частицы в заданной экранной точке
// ================================================================
inline void spawn(float sx, float sy, ImU32 base_color) {
    // Отвязываем alpha от base_color — будем управлять сами
    int spawned = 0;
    int idx = 0;

    // Ищем свободные слоты в пуле (не сбрасываем весь пул!)
    while (spawned < PARTICLES_PER_DEATH && idx < MAX_PARTICLES) {
        if (!g_pool[idx].alive) {
            Particle& p = g_pool[idx];

            // Направление: равномерно по кругу
            float angle = rng_range(0.f, 6.28318530f);
            // Скорость: разброс — быстрые и медленные
            float speed = rng_range(60.f, 280.f);

            p.x        = sx + rng_range(-8.f, 8.f);  // небольшой разброс стартовой позиции
            p.y        = sy + rng_range(-8.f, 8.f);
            p.vx       = cosf(angle) * speed;
            p.vy       = sinf(angle) * speed - rng_range(40.f, 120.f); // небольшой начальный импульс вверх
            p.radius   = rng_range(RADIUS_MIN, RADIUS_MAX);
            p.born     = (float)ImGui::GetTime();
            p.lifetime = rng_range(PARTICLE_LIFETIME * 0.6f, PARTICLE_LIFETIME);
            p.color    = base_color;
            p.alive    = true;

            spawned++;
        }
        idx++;
    }
}

// ================================================================
// UPDATE + RENDER — вызывается каждый кадр
// ================================================================
inline void update_and_render(ImDrawList* dl) {
    if (!dl) return;

    float now = (float)ImGui::GetTime();
    // dt через статику: ImGui::GetIO().DeltaTime
    float dt  = ImGui::GetIO().DeltaTime;
    // Защита от слишком большого dt (пауза/переход)
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.f)  dt = 0.f;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = g_pool[i];
        if (!p.alive) continue;

        float age = now - p.born;
        if (age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        // --- Физика ---
        p.vy += GRAVITY * dt;   // гравитация
        p.vx *= (1.f - 1.5f * dt); // слабое трение по X
        p.x  += p.vx * dt;
        p.y  += p.vy * dt;

        // --- Убиваем вышедшие за экран ---
        if (p.x < -20.f || p.x > g_sw + 20.f ||
            p.y < -20.f || p.y > g_sh + 80.f) {
            p.alive = false;
            continue;
        }

        // --- Alpha fade: 1.0 → 0.0 к концу жизни ---
        float t     = age / p.lifetime;
        // Easing: держим яркость дольше, быстро гасим в конце
        float alpha = (t < 0.7f) ? 1.f : (1.f - (t - 0.7f) / 0.3f);
        if (alpha < 0.f) alpha = 0.f;

        // --- Радиус уменьшается к концу ---
        float cur_r = p.radius * (1.f - t * 0.5f);
        if (cur_r < 1.f) cur_r = 1.f;

        // --- Финальный цвет с alpha ---
        int base_r = (p.color >> IM_COL32_R_SHIFT) & 0xFF;
        int base_g = (p.color >> IM_COL32_G_SHIFT) & 0xFF;
        int base_b = (p.color >> IM_COL32_B_SHIFT) & 0xFF;
        int a_val  = (int)(alpha * 255.f);

        ImU32 draw_col = IM_COL32(base_r, base_g, base_b, a_val);

        // Тень
        dl->AddCircleFilled(
            ImVec2(p.x + 1.5f, p.y + 1.5f),
            cur_r + 1.f,
            IM_COL32(0, 0, 0, (int)(alpha * 120.f)));

        // Основная частица
        dl->AddCircleFilled(
            ImVec2(p.x, p.y),
            cur_r,
            draw_col);

        // Тонкий контур для объёма
        dl->AddCircle(
            ImVec2(p.x, p.y),
            cur_r,
            IM_COL32(255, 255, 255, (int)(alpha * 40.f)),
            0, 1.f);
    }
}

// ================================================================
// TRACK — вызывать каждый кадр для каждого видимого игрока
// чтобы поймать момент смерти (hp > 0 → hp == 0)
//
// sx, sy — экранная позиция (центр бокса игрока)
// ================================================================
inline void track(uint64_t player_ptr, int current_hp,
                  float sx, float sy, ImU32 particle_color) {
    if (!g_initialized) init();

    // Ищем существующий трек для этого ptr
    int found = -1;
    int free_slot = -1;

    for (int i = 0; i < MAX_DEATH_TRACKS; i++) {
        if (g_tracks[i].used && g_tracks[i].ptr == player_ptr) {
            found = i;
            break;
        }
        if (!g_tracks[i].used && free_slot < 0) {
            free_slot = i;
        }
    }

    if (found >= 0) {
        // Уже следим за этим игроком
        int prev_hp = g_tracks[found].last_hp;

        // Момент смерти: был жив, стал <= 0
        if (prev_hp > 0 && current_hp <= 0) {
            spawn(sx, sy, particle_color);
        }

        g_tracks[found].last_hp = current_hp;

    } else {
        // Новый игрок — регистрируем
        if (free_slot >= 0) {
            g_tracks[free_slot].ptr     = player_ptr;
            g_tracks[free_slot].last_hp = current_hp;
            g_tracks[free_slot].used    = true;
        }
        // Если пул треков полный — молча пропускаем (нет heap)
    }
}

// ================================================================
// FLUSH DEAD TRACKS — чистим записи для ptr которых больше нет
// Вызывать раз в кадр ПОСЛЕ обхода всего PlayerList
// ================================================================
// Маркер "видели на этом кадре" — сбрасываем снаружи
static bool g_seen_this_frame[MAX_DEATH_TRACKS];

inline void begin_frame() {
    memset(g_seen_this_frame, 0, sizeof(g_seen_this_frame));
}

inline void mark_seen(uint64_t player_ptr) {
    for (int i = 0; i < MAX_DEATH_TRACKS; i++) {
        if (g_tracks[i].used && g_tracks[i].ptr == player_ptr) {
            g_seen_this_frame[i] = true;
            return;
        }
    }
}

inline void end_frame() {
    // Трек не был отмечен несколько кадров подряд → игрок ушёл → чистим
    // Простой вариант: если не видели — сбрасываем
    for (int i = 0; i < MAX_DEATH_TRACKS; i++) {
        if (g_tracks[i].used && !g_seen_this_frame[i]) {
            g_tracks[i].used = false;
            g_tracks[i].ptr  = 0;
        }
    }
}

// ================================================================
// FORCE CLEAR — для сброса всех частиц (если эффект отключили)
// ================================================================
inline void clear_all() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_pool[i].alive = false;
    }
}

} // namespace death_effect
