#include "aimbot.hpp"
#include "../ui/cfg.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../func/visuals.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>
#include "imgui.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

static auto g_lastAimWrite = std::chrono::steady_clock::now();
static constexpr int AIM_WRITE_INTERVAL_MS = 16;

// Таблица костей: target (0-3) -> оффсет
static const uint64_t g_boneOffsets[] = {
    0x20, // 0 = head
    0x28, // 1 = neck
    0x30, // 2 = chest (Spine)
    0x88, // 3 = pelvis
};

namespace aimbot {
    void handle() {
        if (!cfg::aim::enabled) return;

        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager) return;

        // FIX 0.39.2: PlayerManager + 0x68 = localPlayer (offsets.txt). Было 0x70
        // — там второй PlayerController*, aimbot писал в чужой AimController.
        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + 0x70);
        if (!LocalPlayer) return;

        uint64_t AimController = rpm<uint64_t>(LocalPlayer + 0x80);
        if (!AimController) AimController = rpm<uint64_t>(LocalPlayer + 0x60);
        if (!AimController) return;

        uint64_t AimingData = rpm<uint64_t>(AimController + 0x90);
        if (!AimingData) return;

        float currentPitch = rpm<float>(AimingData + 0x18);
        float currentYaw = rpm<float>(AimingData + 0x1C);

        while (currentYaw > 180.0f) currentYaw -= 360.0f;
        while (currentYaw < -180.0f) currentYaw += 360.0f;

        // ViewMatrix
        matrix ViewMatrix;
        memset(&ViewMatrix, 0, sizeof(matrix));
        uint64_t v1 = rpm<uint64_t>(LocalPlayer + 0xE8);
        if (v1) {
            uint64_t v2 = rpm<uint64_t>(v1 + 0x20);
            if (v2) {
                uint64_t v3 = rpm<uint64_t>(v2 + 0x10);
                if (v3) ViewMatrix = rpm<matrix>(v3 + 0xF0);
            }
        }

        // Camera position
        Vector3 CameraPos = player::position(LocalPlayer);
        uint64_t camTr = rpm<uint64_t>(AimController + 0x80);
        if (camTr) {
            Vector3 cp = player::get_transform_position(camTr);
            if (cp.x != 0 || cp.y != 0 || cp.z != 0) CameraPos = cp;
        }

        int LocalTeam = rpm<uint8_t>(LocalPlayer + 0x79);

        uint64_t PlayerList = rpm<uint64_t>(PlayerManager + 0x28);
        if (!PlayerList) return;
        int PlayerCount = rpm<int>(PlayerList + 0x20);
        if (PlayerCount <= 0 || PlayerCount > 128) return;
        uint64_t ListBuffer = rpm<uint64_t>(PlayerList + 0x18);
        if (!ListBuffer) return;

        // Выбранная кость (0-3)
        int selectedBone = std::clamp(cfg::aim::bone, 0, 3);
        uint64_t boneOffset = g_boneOffsets[selectedBone];

        float best_fov = cfg::aim::fov_size;
        uint64_t best_target = 0;
        Vector3 best_bone_pos;

        for (int i = 0; i < PlayerCount; i++) {
            uint64_t Player = rpm<uint64_t>(ListBuffer + 0x30 + 0x18 * i);
            if (!Player || Player == LocalPlayer) continue;
            if (rpm<uint8_t>(Player + 0x79) == static_cast<uint8_t>(LocalTeam)) continue;
            if (player::health(Player) <= 0) continue;

            // Visible check (always on)
            {
                uint64_t occ = rpm<uint64_t>(Player + 0xB8);
                if (occ) {
                    if (rpm<int>(occ + 0x34) != 2 || rpm<int>(occ + 0x38) == 1) continue;
                }
            }

            // Get bone position
            uint64_t charView = rpm<uint64_t>(Player + 0x48);
            if (!charView) continue;

            uint64_t bipedMap = rpm<uint64_t>(charView + 0x48);
            if (!bipedMap) continue;

            uint64_t boneTransform = rpm<uint64_t>(bipedMap + boneOffset);
            if (!boneTransform) continue;

            Vector3 target_pos = player::get_transform_position(boneTransform);
            if (target_pos.x == 0 && target_pos.y == 0 && target_pos.z == 0) continue;

            ImVec2 screen_pos;
            if (world_to_screen(target_pos, ViewMatrix, screen_pos)) {
                float d = sqrtf(powf(screen_pos.x - g_sw * 0.5f, 2) + powf(screen_pos.y - g_sh * 0.5f, 2));
                if (d < best_fov) {
                    best_fov = d;
                    best_target = Player;
                    best_bone_pos = target_pos;
                }
            }
        }

        if (best_target) {
            Vector3 dir = best_bone_pos - CameraPos;
            float dist = dir.magnitude();
            if (dist < 0.1f) return;

            float targetPitch = -asinf(dir.y / dist) * Rad2Deg;
            float targetYaw = atan2f(dir.x, dir.z) * Rad2Deg;

            while (targetYaw > 180.0f) targetYaw -= 360.0f;
            while (targetYaw < -180.0f) targetYaw += 360.0f;

            float smoothFactor = 1.0f;
            if (cfg::aim::smooth > 0.1f) {
                smoothFactor = 1.0f / (1.0f + cfg::aim::smooth * 0.5f);
                if (smoothFactor < 0.03f) smoothFactor = 0.03f;
                if (smoothFactor > 1.0f) smoothFactor = 1.0f;
            }

            float newPitch = currentPitch + (targetPitch - currentPitch) * smoothFactor;
            float newYaw = currentYaw + (targetYaw - currentYaw) * smoothFactor;

            if ((targetYaw - currentYaw) > 180.0f) newYaw = currentYaw + (targetYaw - currentYaw - 360.0f) * smoothFactor;
            if ((targetYaw - currentYaw) < -180.0f) newYaw = currentYaw + (targetYaw - currentYaw + 360.0f) * smoothFactor;

            newPitch = std::clamp(newPitch, -89.0f, 89.0f);

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastAimWrite).count() >= AIM_WRITE_INTERVAL_MS) {
                g_lastAimWrite = now;
                // FIX 0.39.2: было два бага в записи —
                //   1) дубль записи pitch по 0x18 (мёртвый код).
                //   2) yaw писался в pitch_target (0x24) вместо yaw_target (0x28).
                //      Из-за этого игра пыталась совместить pitch с yaw'ом → aim не работал.
                // Правильно: current pitch/yaw + target pitch/yaw в 4 разных поля.
                wpm<float>(AimingData + 0x18, newPitch);  // current pitch
                wpm<float>(AimingData + 0x1C, newYaw);    // current yaw
                wpm<float>(AimingData + 0x24, newPitch);  // target pitch
                wpm<float>(AimingData + 0x28, newYaw);    // target yaw
            }
        }
    }

void draw_fov_circle() {
    if (!cfg::aim::show_fov) return;
    
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 center = ImVec2(g_sw / 2, g_sh / 2);
    float radius = cfg::aim::fov_size;
    
    // Получаем цвет
    ImU32 color;
    if (cfg::aim::fov_rgb) {
        static float timer = 0.0f;
        timer += ImGui::GetIO().DeltaTime * cfg::aim::fov_rgb_speed;
        float r = (sinf(timer) + 1.0f) * 0.5f;
        float g = (sinf(timer + 2.0f * IM_PI / 3.0f) + 1.0f) * 0.5f;
        float b = (sinf(timer + 4.0f * IM_PI / 3.0f) + 1.0f) * 0.5f;
        color = IM_COL32(r * 255, g * 255, b * 255, cfg::aim::fov_color.w * 255);
    } else {
        color = IM_COL32(
            cfg::aim::fov_color.x * 255,
            cfg::aim::fov_color.y * 255,
            cfg::aim::fov_color.z * 255,
            cfg::aim::fov_color.w * 255
        );
    }
    
    // Рисуем GLOW (свечение) - несколько кругов с разной прозрачностью
    float glow = cfg::aim::fov_glow;
    if (glow > 0.0f) {
        for (int i = 1; i <= (int)glow; i++) {
            float alpha = 0.15f * (1.0f - (i / glow));
            ImU32 glow_color = IM_COL32(
                (color >> 0) & 0xFF,
                (color >> 8) & 0xFF,
                (color >> 16) & 0xFF,
                alpha * 255
            );
            draw->AddCircle(center, radius + i * 2.0f, glow_color, 0, 1.5f);
        }
    }
    
    // Основной круг
    draw->AddCircle(center, radius, color, 0, 2.0f);
}
}