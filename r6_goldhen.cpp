// // FILL: dump r6 eboot/module offsets in IDA/Ghidra for your exact game build & firmware

#include <cstdint>
#include <cstring>
#include <cmath>

namespace cfg {
    constexpr bool     kAimEnabled   = false;
    constexpr bool     kEspEnabled   = false;
    constexpr float    kFovPx        = 120.0f;
    constexpr float    kSmoothing    = 6.0f;
    constexpr int      kHeadBoneId   = 6;       // // FILL: head bone index from skeleton dump
    constexpr bool     kTeamCheck    = true;
    constexpr uint32_t kTickMs       = 4;
    constexpr int      kScreenW      = 1920;
    constexpr int      kScreenH      = 1080;
}

namespace off {
    constexpr uintptr_t kEbootBase       = 0x00400000; // // FILL: verify eboot base address
    constexpr uintptr_t kGameManager     = 0x0;        // // FILL: dump GameManager ptr offset
    constexpr uintptr_t kEntityList      = 0x0;        // // FILL: dump entity array offset in GameManager
    constexpr uintptr_t kEntityCount     = 0x0;        // // FILL: dump entity count offset
    constexpr uintptr_t kLocalPlayer     = 0x0;        // // FILL: dump local player ptr offset
    constexpr uintptr_t kViewMatrix      = 0x0;        // // FILL: dump 4x4 view/proj matrix offset
    constexpr uintptr_t kBoneArray       = 0x0;        // // FILL: dump bone matrix array offset on entity
    constexpr uintptr_t kTeamId          = 0x0;        // // FILL: dump team id offset
    constexpr uintptr_t kHealth          = 0x0;        // // FILL: dump health/hp offset
    constexpr uintptr_t kSetViewAnglesFn = 0x0;        // // FILL: dump angle write/update fn RVA if used
}

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Mat4 { float m[16]; };

struct FeatureState {
    bool aim_toggle = cfg::kAimEnabled;
    bool esp_toggle = cfg::kEspEnabled;
    bool running    = true;
};

static FeatureState g_state;

// row-major 4x4 clip projection; // FILL: confirm engine matrix layout (row vs col major)
static bool WorldToScreen(const Mat4& vp, const Vec3& pos, Vec2& out) {
    float clip_x = pos.x * vp.m[0] + pos.y * vp.m[4] + pos.z * vp.m[8]  + vp.m[12];
    float clip_y = pos.x * vp.m[1] + pos.y * vp.m[5] + pos.z * vp.m[9]  + vp.m[13];
    float clip_w = pos.x * vp.m[3] + pos.y * vp.m[7] + pos.z * vp.m[11] + vp.m[15];

    if (clip_w < 0.01f) {
        return false;
    }

    float ndc_x = clip_x / clip_w;
    float ndc_y = clip_y / clip_w;

    out.x = (cfg::kScreenW * 0.5f) * (ndc_x + 1.0f);
    out.y = (cfg::kScreenH * 0.5f) * (1.0f - ndc_y);
    return (out.x >= 0.0f && out.x <= cfg::kScreenW && out.y >= 0.0f && out.y <= cfg::kScreenH);
}

static bool ReadSafePtr(uintptr_t addr, uintptr_t& out_val) {
    if (!addr) return false;
    // // FILL: validate mapped memory range / use safe read helper from dump
    out_val = *reinterpret_cast<const uintptr_t*>(addr);
    return out_val != 0;
}

static void DrawBox2D(float x, float y, float w, float h, uint32_t rgba) {
    (void)x; (void)y; (void)w; (void)h; (void)rgba;
    // // FILL: hook game renderer / video flip / present swapchain and draw primitives here
}

static void RunEspPass() {
    if (!g_state.esp_toggle || off::kGameManager == 0 || off::kViewMatrix == 0) {
        return;
    }

    uintptr_t gm = 0;
    if (!ReadSafePtr(off::kEbootBase + off::kGameManager, gm)) return;

    const auto* vp = reinterpret_cast<const Mat4*>(off::kEbootBase + off::kViewMatrix);
    uintptr_t entities = 0;
    if (!ReadSafePtr(gm + off::kEntityList, entities)) return;

    uint32_t count = *reinterpret_cast<const uint32_t*>(gm + off::kEntityCount);
    if (count > 64) count = 64;

    for (uint32_t i = 0; i < count; ++i) {
        uintptr_t ent = 0;
        if (!ReadSafePtr(entities + i * sizeof(uintptr_t), ent)) continue;

        int hp = *reinterpret_cast<const int*>(ent + off::kHealth);
        if (hp <= 0) continue;

        // // FILL: verify bone struct layout and stride for head/feet positions
        uintptr_t bones = 0;
        if (!ReadSafePtr(ent + off::kBoneArray, bones)) continue;

        const auto* head_bone = reinterpret_cast<const Vec3*>(bones + cfg::kHeadBoneId * 0x30); // // FILL: bone stride
        Vec2 screen_head{};
        if (WorldToScreen(*vp, *head_bone, screen_head)) {
            DrawBox2D(screen_head.x - 10.0f, screen_head.y - 10.0f, 20.0f, 40.0f, 0xFF00FF00);
        }
    }
}

static void RunAimPass() {
    if (!g_state.aim_toggle || off::kGameManager == 0 || off::kViewMatrix == 0) {
        return;
    }

    uintptr_t gm = 0;
    if (!ReadSafePtr(off::kEbootBase + off::kGameManager, gm)) return;

    uintptr_t local_ent = 0;
    if (!ReadSafePtr(gm + off::kLocalPlayer, local_ent)) return;

    uint32_t local_team = cfg::kTeamCheck ? *reinterpret_cast<const uint32_t*>(local_ent + off::kTeamId) : 0;
    const auto* vp = reinterpret_cast<const Mat4*>(off::kEbootBase + off::kViewMatrix);

    uintptr_t entities = 0;
    if (!ReadSafePtr(gm + off::kEntityList, entities)) return;

    uint32_t count = *reinterpret_cast<const uint32_t*>(gm + off::kEntityCount);
    if (count > 64) count = 64;

    const float cx = cfg::kScreenW * 0.5f;
    const float cy = cfg::kScreenH * 0.5f;
    float best_dist = cfg::kFovPx;
    Vec2 best_target{0.0f, 0.0f};
    bool found = false;

    for (uint32_t i = 0; i < count; ++i) {
        uintptr_t ent = 0;
        if (!ReadSafePtr(entities + i * sizeof(uintptr_t), ent) || ent == local_ent) continue;

        int hp = *reinterpret_cast<const int*>(ent + off::kHealth);
        if (hp <= 0) continue;

        if (cfg::kTeamCheck) {
            uint32_t team = *reinterpret_cast<const uint32_t*>(ent + off::kTeamId);
            if (team == local_team) continue;
        }

        uintptr_t bones = 0;
        if (!ReadSafePtr(ent + off::kBoneArray, bones)) continue;

        const auto* head_bone = reinterpret_cast<const Vec3*>(bones + cfg::kHeadBoneId * 0x30); // // FILL: bone stride
        Vec2 pt{};
        if (!WorldToScreen(*vp, *head_bone, pt)) continue;

        float dx = pt.x - cx;
        float dy = pt.y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < best_dist) {
            best_dist = dist;
            best_target = pt;
            found = true;
        }
    }

    if (found) {
        float step_x = (best_target.x - cx) / cfg::kSmoothing;
        float step_y = (best_target.y - cy) / cfg::kSmoothing;
        (void)step_x;
        (void)step_y;
        // // FILL: route smoothed delta into controller/aim input path rather than raw snap writes
    }
}

extern "C" int module_start(size_t, const void*) {
    RunEspPass();
    RunAimPass();
    return 0;
}

extern "C" int module_stop(size_t, const void*) {
    g_state.running = false;
    return 0;
}
