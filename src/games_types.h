#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "raylib.h"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

enum class Language {
    ES,
    EN
};

inline const char* Tr(Language lang, const char* es, const char* en) {
    return (lang == Language::ES) ? es : en;
}

// ============================================================================
// TIPOGRAFÍA GRUESA DE ALTA LEGIBILIDAD (BOLD + SOMBRA SUAVE)
// ============================================================================
inline Font g_uiFont = { 0 };
inline bool g_hasCustomFont = false;

inline bool g_pixelFont = false;   // VT323: toda la interfaz en tipografía pixel
inline Font g_titleFont = { 0 };    // Press Start 2P: títulos grandes

inline void LoadGameFont() {
    std::vector<int> codepoints;
    for (int cp = 32; cp <= 126; ++cp) codepoints.push_back(cp);
    for (int cp = 160; cp <= 255; ++cp) codepoints.push_back(cp); // ¡ ¿ Á É Í Ó Ú Ü Ñ á é í ó ú ü ñ
    for (int cp : { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x20AC }) codepoints.push_back(cp); // – — ‘ ’ “ ” … €

    // 1) Tipografía pixel del proyecto (la misma familia que usan los problemas del viaje)
    const std::string pixelRel = "assets/fonts/VT323-Regular.ttf";
    const std::string pixelCandidates[] = { pixelRel, std::string(GetApplicationDirectory()) + pixelRel };
    for (const auto& path : pixelCandidates) {
        if (FileExists(path.c_str())) {
            g_uiFont = LoadFontEx(path.c_str(), 64, codepoints.data(), static_cast<int>(codepoints.size()));
            if (g_uiFont.texture.id > 0) {
                SetTextureFilter(g_uiFont.texture, TEXTURE_FILTER_BILINEAR);
                g_hasCustomFont = true;
                g_pixelFont = true;
                const std::string titleRel = "assets/fonts/PressStart2P-Regular.ttf";
                const std::string titlePath = FileExists(titleRel.c_str()) ? titleRel : std::string(GetApplicationDirectory()) + titleRel;
                if (FileExists(titlePath.c_str())) {
                    g_titleFont = LoadFontEx(titlePath.c_str(), 48, codepoints.data(), static_cast<int>(codepoints.size()));
                    if (g_titleFont.texture.id > 0) SetTextureFilter(g_titleFont.texture, TEXTURE_FILTER_POINT);
                }
                return;
            }
        }
    }

    // 2) Si faltara el archivo, fuentes del sistema en negrita
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        "C:/Windows/Fonts/verdanab.ttf",
        "C:/Windows/Fonts/segoeui.ttf"
    };

    for (const char* path : candidates) {
        if (FileExists(path)) {
            g_uiFont = LoadFontEx(path, 38, codepoints.data(), static_cast<int>(codepoints.size()));
            if (g_uiFont.texture.id > 0) {
                SetTextureFilter(g_uiFont.texture, TEXTURE_FILTER_BILINEAR);
                g_hasCustomFont = true;
                return;
            }
        }
    }

    g_uiFont = GetFontDefault();
    g_hasCustomFont = false;
}

inline void UnloadGameFont() {
    if (g_hasCustomFont && g_uiFont.texture.id > 0) {
        UnloadFont(g_uiFont);
        g_hasCustomFont = false;
        g_pixelFont = false;
    }
    if (g_titleFont.texture.id > 0) {
        UnloadFont(g_titleFont);
        g_titleFont = Font{};
    }
}

// La fuente pixel es más estrecha y baja que las del sistema para las que se maquetó la interfaz:
// se dibuja un poco más grande y se sube para que la línea base caiga en el mismo sitio.
constexpr float kPixelFontScale = 1.30f;
constexpr float kPixelFontRise  = 0.23f;

inline void DrawUIText(const char* text, float x, float y, float fontSize, Color color, bool shadow = true) {
    if (g_hasCustomFont) {
        float size = fontSize, spacing = 1.0f, off = 1.5f;
        if (g_pixelFont) {
            size = fontSize * kPixelFontScale;
            y -= fontSize * kPixelFontRise;
            spacing = 0.0f;
            off = std::max(1.5f, fontSize * 0.07f);
        }
        if (shadow && (color.r > 50 || color.g > 50 || color.b > 50)) {
            DrawTextEx(g_uiFont, text, { x + off, y + off }, size, spacing, Color{ 0, 0, 0, 185 });
        }
        DrawTextEx(g_uiFont, text, { x, y }, size, spacing, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(fontSize), color);
    }
}

inline float MeasureUIText(const char* text, float fontSize) {
    if (g_hasCustomFont) {
        if (g_pixelFont) return MeasureTextEx(g_uiFont, text, fontSize * kPixelFontScale, 0.0f).x;
        return MeasureTextEx(g_uiFont, text, fontSize, 1.0f).x;
    }
    return static_cast<float>(MeasureText(text, static_cast<int>(fontSize)));
}

// Títulos grandes en la tipografía de bloques (si no está, se usa la normal)
inline float MeasureTitleText(const char* text, float fontSize) {
    if (g_titleFont.texture.id > 0) return MeasureTextEx(g_titleFont, text, fontSize, 0.0f).x;
    return MeasureUIText(text, fontSize);
}
inline void DrawTitleText(const char* text, float x, float y, float fontSize, Color color) {
    if (g_titleFont.texture.id > 0) {
        float off = std::max(3.0f, fontSize * 0.09f);
        DrawTextEx(g_titleFont, text, { x + off, y + off }, fontSize, 0.0f, Color{ 34, 26, 52, 255 });
        DrawTextEx(g_titleFont, text, { x, y }, fontSize, 0.0f, color);
    } else {
        DrawUIText(text, x, y, fontSize, color);
    }
}

inline void DrawUITextBoxed(const char* text, Rectangle bounds, float fontSize, float lineSpacing, Color color, bool shadow = false) {
    std::string raw(text);
    std::istringstream paragraphStream(raw);
    std::string paragraph;
    float curY = bounds.y;

    while (std::getline(paragraphStream, paragraph, '\n')) {
        if (paragraph.empty()) {
            curY += fontSize + lineSpacing;
            continue;
        }

        std::istringstream wordStream(paragraph);
        std::string word;
        std::string currentLine;

        while (wordStream >> word) {
            std::string candidate = currentLine.empty() ? word : (currentLine + " " + word);
            if (MeasureUIText(candidate.c_str(), fontSize) <= bounds.width) {
                currentLine = candidate;
            } else {
                if (!currentLine.empty()) {
                    if (curY + fontSize <= bounds.y + bounds.height) {
                        DrawUIText(currentLine.c_str(), bounds.x, curY, fontSize, color, shadow);
                    }
                    curY += fontSize + lineSpacing;
                }
                currentLine = word;
            }
        }

        if (!currentLine.empty()) {
            if (curY + fontSize <= bounds.y + bounds.height) {
                DrawUIText(currentLine.c_str(), bounds.x, curY, fontSize, color, shadow);
            }
            curY += fontSize + lineSpacing;
        }
    }
}

// Interpolación suave de color: Rojo (0.0) -> Ámbar/Amarillo (0.5) -> Verde (1.0)
inline Color GetApprovalColor(float value) {
    float t = std::clamp(value, 0.0f, 1.0f);
    Color red    = { 225, 48, 48, 255 };
    Color yellow = { 245, 200, 45, 255 };
    Color green  = { 46, 215, 95, 255 };

    if (t < 0.5f) {
        float k = t / 0.5f;
        return Color{
            static_cast<unsigned char>(red.r + (yellow.r - red.r) * k),
            static_cast<unsigned char>(red.g + (yellow.g - red.g) * k),
            static_cast<unsigned char>(red.b + (yellow.b - red.b) * k),
            255
        };
    } else {
        float k = (t - 0.5f) / 0.5f;
        return Color{
            static_cast<unsigned char>(yellow.r + (green.r - yellow.r) * k),
            static_cast<unsigned char>(yellow.g + (green.g - yellow.g) * k),
            static_cast<unsigned char>(yellow.b + (green.b - yellow.b) * k),
            255
        };
    }
}

// ============================================================================
// ESTADOS Y ESTRUCTURAS DE MISIÓN
// ============================================================================
enum class GameState {
    MENU,
    LOADING,
    COMMITTEE,
    HANGAR,
    FLIGHT
};

enum class PartCategory {
    CAPSULE,
    LANDER,
    TANK,
    ENGINE,
    BOOSTER
};

struct RocketPart {
    int id = 0;
    std::string name;
    PartCategory category = PartCategory::CAPSULE;
    float costMillions = 0.0f;
    float dryMassTons = 0.0f;
    float fuelMassTons = 0.0f;
    float thrustKN = 0.0f;
    std::string nasaRef;
};

struct RocketStats {
    RocketPart capsule;
    RocketPart lander;
    RocketPart tank;
    RocketPart engine;
    RocketPart booster;

    float totalCost = 0.0f;
    float dryMassTons = 0.0f;
    float maxFuelTons = 0.0f;
    float thrustKN = 0.0f;
    float twr = 0.0f;
    float deltaV = 0.0f;
};

struct SharedMissionData {
    Language lang = Language::ES;
    int activeSaveSlot = 1;
    GameState savedPhase = GameState::COMMITTEE;

    float awardedBudget = 0.0f;
    float remainingBudget = 0.0f;

    int eqCapsuleIdx = -1;
    int eqLanderIdx  = -1;
    int eqTankIdx    = -1;
    int eqEngineIdx  = -1;
    int eqBoosterIdx = 24;

    RocketStats rocket;
    bool missionSuccess = false;
    std::string endReason = "";

    // Provisiones compradas en el hangar (ids de backpack_items.h) para los problemas del viaje
    std::vector<std::string> backpack;

    // Dificultad elegida ante el comité (0 cadete, 1 ingeniero, 2 comandante): también ajusta los problemas del viaje
    int difficulty = 1;

    void Reset() {
        difficulty = 1;
        savedPhase = GameState::COMMITTEE;
        awardedBudget = 0.0f;
        remainingBudget = 0.0f;
        eqCapsuleIdx = -1;
        eqLanderIdx  = -1;
        eqTankIdx    = -1;
        eqEngineIdx  = -1;
        eqBoosterIdx = 24;
        rocket = RocketStats{};
        missionSuccess = false;
        endReason.clear();
        backpack.clear();
    }
};

// Modelo de cohete terminado según la calidad de las 4 piezas elegidas en el hangar.
// Cada pieza puntúa 0 (básica), 1 (estándar) o 2 (premium); total 0..8:
//   6..8 -> cohete 1 (el mejor)   3..5 -> cohete 2 (equilibrado)   0..2 -> cohete 3 (el peor)
inline int RocketModelFromParts(const SharedMissionData& data) {
    auto level = [](int idx, int base) { return std::clamp((idx - base + 1) / 2, 0, 2); };
    int sum = level(data.eqCapsuleIdx, 0) + level(data.eqTankIdx, 12)
            + level(data.eqEngineIdx, 18) + level(data.eqBoosterIdx, 25);
    if (sum >= 6) return 1;
    if (sum >= 3) return 2;
    return 3;
}

// Vista: el juego se dibuja en 1280x720 lógicos y se escala a la ventana (con bandas si hace falta)
struct ViewFit { float scale = 1.0f; float offX = 0.0f; float offY = 0.0f; };
inline ViewFit FitView(float logicalW, float logicalH) {
    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());
    if (sw <= 0.0f || sh <= 0.0f) { sw = logicalW; sh = logicalH; }
    ViewFit v;
    v.scale = std::min(sw / logicalW, sh / logicalH);
    v.offX = std::floor((sw - logicalW * v.scale) * 0.5f);
    v.offY = std::floor((sh - logicalH * v.scale) * 0.5f);
    return v;
}
