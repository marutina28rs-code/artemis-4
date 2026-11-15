#include "raylib.h"
#include "games_types.h"
#include "save_manager.h"
#include "comitee.h"
#include "lab_shop.h"
#include "mission.h"
#include "sfx.h"
#include "shrimp.h"

#ifdef __EMSCRIPTEN__
// Versión web: las partidas se guardan en el navegador (IndexedDB) para que sigan ahí al volver
#include <emscripten.h>
EM_JS(void, WebMountSaves, (), {
    Module.savesReady = 0;
    try { FS.mkdir('/data'); } catch (e) {}
    FS.mount(IDBFS, {}, '/data');
    FS.syncfs(true, function (err) { Module.savesReady = 1; });
});
EM_JS(int, WebSavesReady, (), { return Module.savesReady ? 1 : 0; });
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct Star {
    float x;
    float y;
    float size;
    unsigned char alpha;
};

int main() {
    // Ventana redimensionable que arranca maximizada: el juego se dibuja siempre en
    // 1280x720 lógicos y se escala a la ventana
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ARTEMIS 4");
    SetWindowMinSize(640, 360);
    MaximizeWindow();
#ifdef __EMSCRIPTEN__
    WebMountSaves();
    while (!WebSavesReady()) emscripten_sleep(16);
#endif
    SfxInit();
    LoadGameFont();
    ShrimpInit();

    Texture2D texMenuSky{};
    if (FileExists("assets/sprites/incidents/space.png")) texMenuSky = LoadTexture("assets/sprites/incidents/space.png");
    Texture2D texMenuMoon{};
    if (FileExists("assets/sprites/incidents/moon.png")) texMenuMoon = LoadTexture("assets/sprites/incidents/moon.png");
    Texture2D texMenuEarth{};
    if (FileExists("assets/sprites/earth.png")) texMenuEarth = LoadTexture("assets/sprites/earth.png");
    Texture2D texMenuRocket{};
    if (FileExists("assets/sprites/rockets/cohete_1.png")) texMenuRocket = LoadTexture("assets/sprites/rockets/cohete_1.png");
    SetTargetFPS(60);

    SharedMissionData missionData{};
    missionData.Reset();
    missionData.lang = Language::ES;
    missionData.activeSaveSlot = 1;

    int selectedSlot = 1;
    GameState currentState = GameState::MENU;

    CommitteePhase committee;
    committee.Init(missionData.lang);

    LabShopPhase labShop;
    labShop.Init();

    MissionPhase mission;
    mission.Init();

    std::vector<Star> stars;
    stars.reserve(140);
    for (int i = 0; i < 140; ++i) {
        stars.push_back({
            static_cast<float>((i * 197 + 43) % SCREEN_WIDTH),
            static_cast<float>((i * 311 + 89) % SCREEN_HEIGHT),
            (i % 3 == 0) ? 2.5f : 1.5f,
            static_cast<unsigned char>(120 + (i * 29) % 135)
        });
    }

    auto launchSlot = [&](int slot) {
        Language keepLang = missionData.lang;
        SaveSlotSummary summary = SaveManager::GetSlotSummary(slot);

        if (summary.occupied) {
            GameState loadedPhase = GameState::COMMITTEE;
            if (SaveManager::LoadGame(slot, missionData, loadedPhase)) {
                missionData.lang = keepLang;
                missionData.activeSaveSlot = slot;
                if (loadedPhase == GameState::HANGAR) {
                    labShop.RestoreFromSave(missionData);
                    currentState = GameState::HANGAR;
                } else if (loadedPhase == GameState::FLIGHT) {
                    labShop.RestoreFromSave(missionData);
                    mission.OnEnter(missionData);
                    currentState = GameState::FLIGHT;
                } else {
                    committee.Init(missionData.lang);
                    currentState = GameState::COMMITTEE;
                }
            }
        } else {
            missionData.Reset();
            missionData.lang = keepLang;
            missionData.activeSaveSlot = slot;
            committee.Init(missionData.lang);
            currentState = GameState::COMMITTEE;
        }
    };

    while (!WindowShouldClose()) {
        // El ratón se convierte a coordenadas lógicas 1280x720 para todas las fases
        ViewFit view = FitView(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
        SetMouseOffset(static_cast<int>(-view.offX), static_cast<int>(-view.offY));
        SetMouseScale(1.0f / view.scale, 1.0f / view.scale);
        Vector2 mouse = GetMousePosition();
        SfxUpdate(GetFrameTime());

        // Huevo de pascua: si la gamba se queda con el clic (o está contando su dato), el juego espera
        bool shrimpBusy = ShrimpUpdate(GetFrameTime(), mouse, missionData.lang);

        if (shrimpBusy) {
            // nada: pausa
        } else if (currentState == GameState::MENU) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Rectangle btnLang = { 24.0f, 20.0f, 115.0f, 34.0f };
                if (CheckCollisionPointRec(mouse, btnLang)) {
                    missionData.lang = (missionData.lang == Language::ES) ? Language::EN : Language::ES;
                }

                for (int slot = 1; slot <= 3; ++slot) {
                    int idx = slot - 1;
                    Rectangle slotBox = { 90.0f, 225.0f + idx * 130.0f, 690.0f, 112.0f };
                    Rectangle delBtn  = { slotBox.x + slotBox.width - 110.0f, slotBox.y + 62.0f, 92.0f, 34.0f };

                    SaveSlotSummary summary = SaveManager::GetSlotSummary(slot);

                    if (summary.occupied && CheckCollisionPointRec(mouse, delBtn)) {
                        SaveManager::DeleteSlot(slot);
                        selectedSlot = slot;
                        break;
                    }

                    if (CheckCollisionPointRec(mouse, slotBox)) {
                        selectedSlot = slot;
                        launchSlot(slot);
                        break;
                    }
                }

                Rectangle rightMainBtn = { 835.0f, 465.0f, 330.0f, 56.0f };
                Rectangle rightDelBtn  = { 835.0f, 535.0f, 330.0f, 40.0f };
                SaveSlotSummary selSummary = SaveManager::GetSlotSummary(selectedSlot);

                if (CheckCollisionPointRec(mouse, rightMainBtn)) {
                    launchSlot(selectedSlot);
                } else if (selSummary.occupied && CheckCollisionPointRec(mouse, rightDelBtn)) {
                    SaveManager::DeleteSlot(selectedSlot);
                }
            }
        } else if (currentState == GameState::COMMITTEE) {
            GameState next = committee.Update(missionData);
            if (next == GameState::HANGAR) {
                labShop.OnEnter(missionData);
                SaveManager::SaveGame(missionData.activeSaveSlot, missionData, GameState::HANGAR);
            }
            currentState = next;
        } else if (currentState == GameState::HANGAR) {
            GameState next = labShop.Update(missionData);
            if (next == GameState::FLIGHT) {
                SaveManager::SaveGame(missionData.activeSaveSlot, missionData, GameState::FLIGHT);
                mission.OnEnter(missionData);
            }
            currentState = next;
        } else if (currentState == GameState::FLIGHT) {
            currentState = mission.Update(missionData);
        } else {
            currentState = GameState::MENU;
        }

        // Las fases pueden haber cambiado el tamaño de la ventana durante Update
        view = FitView(static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT));
        Camera2D viewCam{};
        viewCam.offset = { view.offX, view.offY };
        viewCam.target = { 0.0f, 0.0f };
        viewCam.rotation = 0.0f;
        viewCam.zoom = view.scale;

        BeginDrawing();
        ClearBackground(Color{ 8, 12, 24, 255 });
        BeginMode2D(viewCam);

        if (currentState == GameState::MENU) {
            const Color ink     = { 34, 26, 52, 255 };
            const Color cGold   = { 242, 193, 78, 255 };
            const Color cGreen  = { 88, 179, 104, 255 };
            const Color cSky    = { 127, 209, 255, 255 };
            const Color cDim    = { 201, 208, 240, 255 };
            float tNow = static_cast<float>(GetTime());

            // ---- Fondo: cielo pixel-art, Luna, Tierra y el cohete cruzando por abajo
            if (texMenuSky.id > 0) {
                float skyW = SCREEN_HEIGHT * static_cast<float>(texMenuSky.width) / static_cast<float>(texMenuSky.height);
                float skyX = -std::fmod(tNow * 12.0f, skyW);
                for (int k = 0; k < 2; ++k) {
                    DrawTexturePro(texMenuSky, { 0, 0, (float)texMenuSky.width, (float)texMenuSky.height },
                                   { skyX + k * skyW, 0.0f, skyW, (float)SCREEN_HEIGHT }, { 0, 0 }, 0.0f, WHITE);
                }
            } else {
                for (const auto& s : stars) {
                    DrawCircleV({ s.x, s.y }, s.size, Color{ 230, 240, 255, s.alpha });
                }
            }
            if (texMenuMoon.id > 0) {
                float mw = 150.0f, mh = mw * static_cast<float>(texMenuMoon.height) / static_cast<float>(texMenuMoon.width);
                DrawTexturePro(texMenuMoon, { 0, 0, (float)texMenuMoon.width, (float)texMenuMoon.height },
                               { 1085.0f, 34.0f + std::sin(tNow * 0.8f) * 4.0f, mw, mh }, { 0, 0 }, 0.0f, WHITE);
            }
            if (texMenuEarth.id > 0) {
                float ew = 360.0f, eh = ew * static_cast<float>(texMenuEarth.height) / static_cast<float>(texMenuEarth.width);
                DrawTexturePro(texMenuEarth, { 0, 0, (float)texMenuEarth.width, (float)texMenuEarth.height },
                               { 815.0f, 612.0f, ew, eh }, { 0, 0 }, 0.0f, WHITE);
            }
            if (texMenuRocket.id > 0) {
                float cycle = std::fmod(tNow, 18.0f) / 18.0f;
                float rx = -220.0f + cycle * (SCREEN_WIDTH + 440.0f);
                float ry = 656.0f + std::sin(tNow * 2.0f) * 5.0f;
                float len = 132.0f, thick = len * static_cast<float>(texMenuRocket.width) / static_cast<float>(texMenuRocket.height);
                int rows = 9 + static_cast<int>(2.0f + 2.0f * std::sin(tNow * 24.0f));
                for (int i = 0; i < rows; ++i) {
                    float u = static_cast<float>(i) / rows;
                    float half = (1.0f - u) * 13.0f + 2.0f;
                    float fx = rx - len * 0.5f - 2.0f - i * 8.0f;
                    float fy = ry + std::sin(tNow * 18.0f - i * 0.6f) * u * 5.0f;
                    DrawRectangleRec({ fx - 8.0f, fy - half - 3.0f, 9.0f, 2.0f * half + 6.0f }, Color{ 158, 42, 28, 255 });
                    DrawRectangleRec({ fx - 8.0f, fy - half, 9.0f, 2.0f * half }, Color{ 232, 99, 44, 255 });
                    if (u < 0.7f)  DrawRectangleRec({ fx - 8.0f, fy - half * 0.6f, 9.0f, 2.0f * half * 0.6f }, cGold);
                    if (u < 0.35f) DrawRectangleRec({ fx - 8.0f, fy - half * 0.3f, 9.0f, 2.0f * half * 0.3f }, Color{ 255, 243, 176, 255 });
                }
                DrawTexturePro(texMenuRocket, { 0, 0, (float)texMenuRocket.width, (float)texMenuRocket.height },
                               { rx, ry, thick, len }, { thick * 0.5f, len * 0.5f }, 90.0f, WHITE);
            }

            // ---- Piezas de interfaz con borde grueso, como el resto del arte
            auto pixelPanel = [&](Rectangle r, Color fill, Color border) {
                DrawRectangleRec({ r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f }, ink);
                DrawRectangleRec(r, border);
                DrawRectangleRec({ r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, r.height - 8.0f }, fill);
                DrawRectangleRec({ r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, 4.0f }, Color{ 255, 255, 255, 22 });
            };
            auto pixelButton = [&](Rectangle r, Color base, const char* label, float size, Color textCol) {
                bool hover = CheckCollisionPointRec(mouse, r);
                auto shade = [](Color c, int d) {
                    return Color{ static_cast<unsigned char>(std::clamp(c.r + d, 0, 255)), static_cast<unsigned char>(std::clamp(c.g + d, 0, 255)),
                                  static_cast<unsigned char>(std::clamp(c.b + d, 0, 255)), 255 };
                };
                Color b = hover ? shade(base, 26) : base;
                DrawRectangleRec({ r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f }, ink);
                DrawRectangleRec(r, b);
                DrawRectangleRec({ r.x, r.y, r.width, 4.0f }, shade(b, 46));
                DrawRectangleRec({ r.x, r.y + r.height - 5.0f, r.width, 5.0f }, shade(b, -42));
                float tw = MeasureUIText(label, size);
                DrawUIText(label, r.x + (r.width - tw) * 0.5f, r.y + (r.height - size) * 0.5f - 1.0f, size, textCol);
            };

            pixelButton({ 24.0f, 20.0f, 115.0f, 34.0f }, Color{ 32, 68, 128, 255 },
                        missionData.lang == Language::ES ? "ESPAÑOL" : "ENGLISH", 18.0f, WHITE);

            // ---- Título
            {
                const char* title = "ARTEMIS 4";
                float ts = 60.0f, tw = MeasureTitleText(title, ts);
                DrawTitleText(title, (SCREEN_WIDTH - tw) * 0.5f, 42.0f, ts, cGold);
                const char* sub = Tr(missionData.lang,
                    "CONVENCE AL COMITÉ · CONSTRUYE EL COHETE · LLEGA A LA LUNA",
                    "WIN THE BUDGET · BUILD THE ROCKET · REACH THE MOON");
                float sw = MeasureUIText(sub, 22.0f);
                DrawUIText(sub, (SCREEN_WIDTH - sw) * 0.5f, 124.0f, 22.0f, cSky);
                DrawRectangleRec({ (SCREEN_WIDTH - sw) * 0.5f, 154.0f, sw, 4.0f }, Color{ 242, 193, 78, 120 });
            }

            DrawUIText(Tr(missionData.lang, "ELIGE UNA PARTIDA", "CHOOSE A SAVE"), 90.0f, 184.0f, 24.0f, WHITE);

            // ---- Las tres partidas
            for (int slot = 1; slot <= 3; ++slot) {
                int idx = slot - 1;
                Rectangle slotBox = { 90.0f, 225.0f + idx * 130.0f, 690.0f, 112.0f };
                bool hover = CheckCollisionPointRec(mouse, slotBox);
                if (hover) selectedSlot = slot;

                SaveSlotSummary summary = SaveManager::GetSlotSummary(slot);
                bool selected = (selectedSlot == slot);
                Color border = summary.occupied ? cGreen : cGold;

                pixelPanel(slotBox, selected ? Color{ 28, 42, 82, 250 } : Color{ 14, 20, 42, 240 }, selected ? border : Color{ border.r, border.g, border.b, 150 });
                if (selected && std::fmod(tNow, 0.8f) < 0.5f) {
                    DrawUIText(">", slotBox.x - 34.0f, slotBox.y + 38.0f, 34.0f, cGold);
                }

                DrawUIText(TextFormat(Tr(missionData.lang, "PARTIDA %d", "SAVE %d"), slot), slotBox.x + 24.0f, slotBox.y + 14.0f, 30.0f, cGold);

                if (summary.occupied) {
                    int phaseNum = 1;
                    const char* phaseLabel = Tr(missionData.lang, "Fase 1: Comité de presupuesto", "Phase 1: Budget committee");
                    if (summary.savedPhase == GameState::HANGAR) {
                        phaseNum = 2;
                        phaseLabel = Tr(missionData.lang, "Fase 2: Hangar de cohetes", "Phase 2: Rocket hangar");
                    } else if (summary.savedPhase == GameState::FLIGHT) {
                        phaseNum = 3;
                        phaseLabel = Tr(missionData.lang, "Fase 3: Vuelo a la Luna", "Phase 3: Flight to the Moon");
                    }

                    // tres casillas: hasta qué fase ha llegado la partida
                    for (int k = 0; k < 3; ++k) {
                        Rectangle dot = { slotBox.x + 200.0f + k * 26.0f, slotBox.y + 22.0f, 18.0f, 18.0f };
                        DrawRectangleRec({ dot.x - 3.0f, dot.y - 3.0f, dot.width + 6.0f, dot.height + 6.0f }, ink);
                        DrawRectangleRec(dot, k < phaseNum ? cGreen : Color{ 40, 50, 84, 255 });
                    }

                    DrawUIText(phaseLabel, slotBox.x + 24.0f, slotBox.y + 52.0f, 21.0f, cSky);
                    DrawUIText(TextFormat(Tr(missionData.lang, "Presupuesto: $%.0fM", "Budget: $%.0fM"), summary.awardedBudget),
                               slotBox.x + 24.0f, slotBox.y + 78.0f, 19.0f, cDim);

                    const char* go = Tr(missionData.lang, "CONTINUAR >", "CONTINUE >");
                    DrawUIText(go, slotBox.x + slotBox.width - MeasureUIText(go, 22.0f) - 24.0f, slotBox.y + 18.0f, 22.0f, cGreen);

                    Rectangle delBtn = { slotBox.x + slotBox.width - 110.0f, slotBox.y + 62.0f, 92.0f, 34.0f };
                    pixelButton(delBtn, Color{ 150, 44, 44, 255 }, Tr(missionData.lang, "BORRAR", "DELETE"), 17.0f, WHITE);
                } else {
                    DrawUIText(Tr(missionData.lang, "Vacía: empieza una misión nueva", "Empty: start a new mission"),
                               slotBox.x + 24.0f, slotBox.y + 60.0f, 21.0f, cDim);

                    const char* go = Tr(missionData.lang, "NUEVA PARTIDA >", "NEW GAME >");
                    DrawUIText(go, slotBox.x + slotBox.width - MeasureUIText(go, 22.0f) - 24.0f, slotBox.y + 44.0f, 22.0f, cGold);
                }
            }

            // ---- Panel de la partida seleccionada
            SaveSlotSummary selSummary = SaveManager::GetSlotSummary(selectedSlot);
            Rectangle rightPanel = { 810.0f, 225.0f, 380.0f, 372.0f };
            pixelPanel(rightPanel, Color{ 12, 18, 38, 248 }, selSummary.occupied ? cGreen : cGold);

            DrawUIText(TextFormat(Tr(missionData.lang, "PARTIDA %d", "SAVE %d"), selectedSlot), 835.0f, 246.0f, 32.0f, cGold);
            DrawRectangleRec({ 835.0f, 286.0f, 330.0f, 4.0f }, Color{ 50, 75, 120, 255 });

            Rectangle rightMainBtn = { 835.0f, 465.0f, 330.0f, 56.0f };
            if (selSummary.occupied) {
                DrawUIText(Tr(missionData.lang, "MISIÓN EN MARCHA", "MISSION IN PROGRESS"), 835.0f, 304.0f, 21.0f, cGreen);

                const char* phaseName = Tr(missionData.lang, "Fase 1 · Comité de presupuesto", "Phase 1 · Budget committee");
                if (selSummary.savedPhase == GameState::HANGAR) {
                    phaseName = Tr(missionData.lang, "Fase 2 · Hangar de cohetes", "Phase 2 · Rocket hangar");
                } else if (selSummary.savedPhase == GameState::FLIGHT) {
                    phaseName = Tr(missionData.lang, "Fase 3 · Vuelo a la Luna", "Phase 3 · Flight to the Moon");
                }

                DrawUIText(phaseName, 835.0f, 338.0f, 21.0f, cSky);
                DrawUIText(TextFormat(Tr(missionData.lang, "Presupuesto: $%.0fM", "Budget: $%.0fM"), selSummary.awardedBudget),
                           835.0f, 372.0f, 21.0f, WHITE);
                DrawUIText(TextFormat(Tr(missionData.lang, "Sin gastar: $%.0fM", "Unspent: $%.0fM"), selSummary.remainingBudget),
                           835.0f, 402.0f, 21.0f, cDim);

                pixelButton(rightMainBtn, Color{ 44, 138, 72, 255 }, Tr(missionData.lang, "CONTINUAR PARTIDA", "CONTINUE GAME"), 26.0f, WHITE);
                pixelButton({ 835.0f, 535.0f, 330.0f, 40.0f }, Color{ 138, 40, 40, 255 }, Tr(missionData.lang, "BORRAR PARTIDA", "DELETE SAVE"), 20.0f, WHITE);
            } else {
                DrawUIText(Tr(missionData.lang, "PARTIDA VACÍA", "EMPTY SAVE"), 835.0f, 304.0f, 21.0f, cGold);
                DrawUITextBoxed(Tr(missionData.lang,
                    "Empiezas ante el comité: convéncelos para conseguir presupuesto, monta tu cohete en el hangar y vuela hasta la Luna.",
                    "You start before the committee: win them over to get a budget, build your rocket in the hangar and fly to the Moon."),
                    { 835.0f, 338.0f, 330.0f, 120.0f }, 20.0f, 5.0f, cDim);

                pixelButton(rightMainBtn, Color{ 36, 92, 170, 255 }, Tr(missionData.lang, "NUEVA PARTIDA", "NEW GAME"), 26.0f, WHITE);
            }

            const char* foot = "NASA SPACE APPS CHALLENGE 2026";
            DrawUIText(foot, 24.0f, 672.0f, 16.0f, Color{ 143, 155, 209, 255 });
            const char* credits = Tr(missionData.lang,
                "CRÉDITOS: Claudia Sanz · Rafael Lizasoain · Gonzalo de Sagarra · Sol Gonzalez · Martina Ruipérez",
                "CREDITS: Claudia Sanz · Rafael Lizasoain · Gonzalo de Sagarra · Sol Gonzalez · Martina Ruipérez");
            DrawUIText(credits, 24.0f, 694.0f, 16.0f, Color{ 201, 208, 240, 255 });
        } else if (currentState == GameState::COMMITTEE) {
            committee.Draw(missionData);
        } else if (currentState == GameState::HANGAR) {
            labShop.Draw(missionData);
        } else if (currentState == GameState::FLIGHT) {
            mission.Draw(missionData);
        }

        ShrimpDraw(missionData.lang);

        EndMode2D();

        // Bandas negras si la ventana no es 16:9
        int winW = GetScreenWidth(), winH = GetScreenHeight();
        if (view.offX > 0.0f) {
            DrawRectangle(0, 0, static_cast<int>(view.offX), winH, BLACK);
            DrawRectangle(static_cast<int>(view.offX + SCREEN_WIDTH * view.scale), 0, winW, winH, BLACK);
        }
        if (view.offY > 0.0f) {
            DrawRectangle(0, 0, winW, static_cast<int>(view.offY), BLACK);
            DrawRectangle(0, static_cast<int>(view.offY + SCREEN_HEIGHT * view.scale), winW, winH, BLACK);
        }

        EndDrawing();
    }

    if (texMenuSky.id > 0) UnloadTexture(texMenuSky);
    if (texMenuMoon.id > 0) UnloadTexture(texMenuMoon);
    if (texMenuEarth.id > 0) UnloadTexture(texMenuEarth);
    if (texMenuRocket.id > 0) UnloadTexture(texMenuRocket);

    committee.Unload();
    labShop.Unload();
    mission.Unload();
    ShrimpUnload();
    UnloadGameFont();
    SfxShutdown();
    CloseWindow();
    return 0;
}