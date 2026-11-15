#include "mission.h"
#include "save_manager.h"
#include "incidents.h"
#include "sfx.h"
#include <algorithm>
#include <cmath>

namespace {
    // Paleta y ayudas para dibujar el despegue con "píxeles" gordos, como el resto del arte
    const Color kInk = { 34, 26, 52, 255 };          // contorno oscuro de los sprites
    float Snap(float v) { return std::floor(v / 4.0f) * 4.0f; }
    void PxRect(float x, float y, float w, float h, Color c) {
        DrawRectangle(static_cast<int>(Snap(x)), static_cast<int>(Snap(y)),
                      static_cast<int>(std::max(4.0f, Snap(w))), static_cast<int>(std::max(4.0f, Snap(h))), c);
    }
    float Hash(int n) { float h = std::sin(n * 12.9898f) * 43758.5453f; return h - std::floor(h); }
    Color Mix(Color a, Color b, float t) {
        return Color{ static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                      static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255 };
    }
    // ---- Tirachinas translunar: posiciones en pantalla y física de juguete
    const Vector2 kEarth = { 185.0f, 390.0f };
    const Vector2 kMoon  = { 1085.0f, 245.0f };
    const Vector2 kStart = { 372.0f, 372.0f };
    const float kSimK    = 1.4f;                          // acelera el vuelo sin cambiar las trayectorias
    const float kGE      = 1.9e7f * kSimK * kSimK;        // "gravedad" de la Tierra
    const float kGM      = 4.0e6f * kSimK * kSimK;        // y de la Luna
    const float kNeed    = 360.0f * kSimK;                // velocidad mínima con la que se puede llegar
    const float kCapture = 100.0f;                        // radio del anillo de captura lunar
    const float kMaxDrag = 170.0f;                        // píxeles de arrastre para potencia máxima
    const float kStep    = 1.0f / 120.0f;

    void GravStep(Vector2& p, Vector2& v, float dt) {
        const Vector2 c[2] = { kEarth, kMoon };
        const float g[2] = { kGE, kGM };
        for (int i = 0; i < 2; ++i) {
            float dx = c[i].x - p.x, dy = c[i].y - p.y;
            float r2 = std::max(400.0f, dx * dx + dy * dy), r = std::sqrt(r2);
            v.x += g[i] / r2 * dx / r * dt;
            v.y += g[i] / r2 * dy / r * dt;
        }
        p.x += v.x * dt;
        p.y += v.y * dt;
    }
    // Velocidad máxima de lanzamiento según el Delta-V del cohete: con menos de 5.800 m/s no se
    // llega; con más, hay más margen (y tiros más directos).
    float SlingMaxSpeed(float deltaV) {
        float r = deltaV / 5800.0f;
        if (r < 1.0f) return kNeed * 0.95f * std::max(0.3f, r);
        return kNeed * (1.10f + 0.20f * std::clamp((r - 1.0f) / 0.25f, 0.0f, 1.0f));
    }
    // Asteroides que cruzan la ruta directa (más cuanto mayor es la dificultad)
    Vector2 RockPos(int i, float t) {
        if (i == 0) return { 640.0f, 300.0f + 150.0f * std::sin(t * 1.3f) };
        if (i == 1) return { 835.0f, 262.0f + 140.0f * std::sin(t * 1.0f + 2.0f) };
        return { 520.0f, 330.0f + 120.0f * std::sin(t * 1.7f + 4.0f) };
    }
    float RockRadius(int i) { return i == 0 ? 26.0f : (i == 1 ? 30.0f : 22.0f); }

    // ---- Escena final: tiempos (segundos)
    const float kEndWalk0 = 1.6f;    // el astronauta baja del módulo y echa a andar a saltos
    const float kEndWalk1 = 5.0f;    // llega al sitio de la bandera
    const float kEndParty = 5.8f;    // bandera izada: empieza la fiesta
    const float kEndHop   = 0.85f;   // duración de cada salto al caminar

    void DrawSpriteCentered(Texture2D t, float cx, float cy, float w) {
        float h = w * static_cast<float>(t.height) / static_cast<float>(t.width);
        DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height }, { cx - w * 0.5f, cy - h * 0.5f, w, h }, { 0, 0 }, 0.0f, WHITE);
    }
}

void MissionPhase::LoadSprites() {
    if (spritesLoaded) return;

    for (int i = 0; i < 6; ++i) {
        const char* pCap = TextFormat("assets/sprites/parts/cap_%d.png", i + 1);
        if (FileExists(pCap)) texCapsules[i] = LoadTexture(pCap);

        const char* pLan = TextFormat("assets/sprites/parts/lander_%d.png", i + 1);
        if (FileExists(pLan)) texLanders[i] = LoadTexture(pLan);

        const char* pTnk = TextFormat("assets/sprites/parts/tank_%d.png", i + 1);
        if (FileExists(pTnk)) texTanks[i] = LoadTexture(pTnk);

        const char* pEng = TextFormat("assets/sprites/parts/eng_%d.png", i + 1);
        if (FileExists(pEng)) texEngines[i] = LoadTexture(pEng);
    }

    for (int i = 0; i < 5; ++i) {
        const char* pBst = TextFormat("assets/sprites/parts/boost_%d.png", i + 1);
        if (FileExists(pBst)) texBoosters[i] = LoadTexture(pBst);
    }

    if (FileExists("assets/sprites/flight/launch_bg.png"))    texLaunchBg    = LoadTexture("assets/sprites/flight/launch_bg.png");
    if (FileExists("assets/sprites/flight/launch_tower.png")) texLaunchTower = LoadTexture("assets/sprites/flight/launch_tower.png");
    if (FileExists("assets/sprites/flight/flame_main_1.png")) texFlameMain1  = LoadTexture("assets/sprites/flight/flame_main_1.png");
    if (FileExists("assets/sprites/flight/flame_main_2.png")) texFlameMain2  = LoadTexture("assets/sprites/flight/flame_main_2.png");
    if (FileExists("assets/sprites/flight/flame_srb_1.png"))  texFlameSrb1   = LoadTexture("assets/sprites/flight/flame_srb_1.png");
    if (FileExists("assets/sprites/flight/flame_srb_2.png"))  texFlameSrb2   = LoadTexture("assets/sprites/flight/flame_srb_2.png");
    if (FileExists("assets/sprites/flight/smoke_cloud.png"))  texSmoke       = LoadTexture("assets/sprites/flight/smoke_cloud.png");

    if (FileExists("assets/sprites/incidents/space.png")) texSky  = LoadTexture("assets/sprites/incidents/space.png");
    if (FileExists("assets/sprites/incidents/moon.png"))  texMoon = LoadTexture("assets/sprites/incidents/moon.png");
    if (FileExists("assets/sprites/earth.png"))           texEarth = LoadTexture("assets/sprites/earth.png");
    if (FileExists("assets/sprites/lander.png"))          texLander = LoadTexture("assets/sprites/lander.png");
    if (FileExists("assets/sprites/astronaut.png"))       texAstronaut = LoadTexture("assets/sprites/astronaut.png");
    if (FileExists("assets/sprites/gamba.png"))           texShrimp = LoadTexture("assets/sprites/gamba.png");
    for (int i = 0; i < 3; ++i) {
        const char* pRocket = TextFormat("assets/sprites/rockets/cohete_%d.png", i + 1);
        if (FileExists(pRocket)) texFinalRocket[i] = LoadTexture(pRocket);
    }

    spritesLoaded = true;
}

// Cielo pixel-art compartido con el viaje y el menú, desplazándose despacio
void MissionPhase::DrawSky(float alpha) const {
    unsigned char a = static_cast<unsigned char>(255.0f * std::clamp(alpha, 0.0f, 1.0f));
    if (texSky.id <= 0) {
        for (const auto& s : stars) {
            DrawCircleV({ s.x, s.y }, s.size, Color{ 235, 242, 255, static_cast<unsigned char>(s.alpha * a / 255) });
        }
        return;
    }
    float skyW = SCREEN_HEIGHT * static_cast<float>(texSky.width) / static_cast<float>(texSky.height);
    float skyX = -std::fmod(static_cast<float>(GetTime()) * 10.0f, skyW);
    for (int k = 0; k < 2; ++k) {
        DrawTexturePro(texSky, { 0, 0, (float)texSky.width, (float)texSky.height },
                       { skyX + k * skyW, 0.0f, skyW, (float)SCREEN_HEIGHT }, { 0, 0 }, 0.0f, Color{ 255, 255, 255, a });
    }
}

void MissionPhase::Init() {
    LoadSprites();

    stars.clear();
    stars.reserve(150);
    for (int i = 0; i < 150; ++i) {
        stars.push_back({
            static_cast<float>((i * 211 + 37) % SCREEN_WIDTH),
            static_cast<float>((i * 331 + 73) % SCREEN_HEIGHT),
            (i % 4 == 0) ? 2.4f : 1.4f,
            static_cast<unsigned char>(130 + (i * 31) % 125)
        });
    }
}

void MissionPhase::OnEnter(const SharedMissionData& data) {
    if (!spritesLoaded || stars.empty()) Init();

    stage = FlightStage::COUNTDOWN_ASCENT;
    countdownTimer = 2.6f; // Cuenta atras rapida y dinamica
    lastCountSec = -1;
    rumbleTimer = 0.0f;
    driftTimer = -1.0f;
    endingTimer = -1.0f;
    sling = SlingState::AIM;
    shipPos = kStart;
    shipVel = { 0.0f, 0.0f };
    dragging = false;
    lowFuelShot = false;
    shotsLeft = (data.difficulty == 0) ? 4 : 3;
    flyTime = 0.0f;
    slingTimer = 0.0f;
    simAccum = 0.0f;
    tliClock = 0.0f;
    shipAngle = 18.0f;
    trail.clear();
    missionClock = 0.0f;
    timeWarp = 1;
    autoPilot = false;

    altitudeKm = 0.0f;
    velocityKms = 0.0f;
    fuelRemainingTons = data.rocket.maxFuelTons;
    throttle = 1.0f;
    pitchDeg = 90.0f;
    padFailTimer = 0.0f;

    boostersAttached = (data.eqBoosterIdx >= 25);
    leftBooster = SeparatedBooster{};
    rightBooster = SeparatedBooster{};

    tliCursor = 0.15f;
    tliProgress = 0.0f;
    tliBurning = false;

    landerAltMeters = 1200.0f;
    landerVelY = -26.0f;
    landerPosX = static_cast<float>((data.activeSaveSlot % 2 == 0) ? -140 : 140);
    landerVelX = 0.0f;
    landerFuelPct = 100.0f;
    touchdownSpeed = 0.0f;

    success = false;
    grade = "B";
    resultTitleEs.clear();
    resultTitleEn.clear();
    resultDetailEs.clear();
    resultDetailEn.clear();
}

void MissionPhase::FinishMission(SharedMissionData& data, bool ok, const std::string& missionGrade,
                                 const std::string& titleEs, const std::string& titleEn,
                                 const std::string& detailEs, const std::string& detailEn) {
    stage = FlightStage::DEBRIEF;
    success = ok;
    endingTimer = ok ? 0.0f : -1.0f;   // si ha alunizado, primero la escena final
    grade = missionGrade;
    resultTitleEs = titleEs;
    resultTitleEn = titleEn;
    resultDetailEs = detailEs;
    resultDetailEn = detailEn;

    data.missionSuccess = ok;
    data.endReason = Tr(data.lang, titleEs.c_str(), titleEn.c_str());
    SaveManager::SaveGame(data.activeSaveSlot, data, GameState::FLIGHT);
}

GameState MissionPhase::Update(SharedMissionData& data) {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_M)) return GameState::MENU;

    // Sonido: pitidos de la cuenta atrás y rugido grave mientras haya motores encendidos
    {
        if (stage == FlightStage::COUNTDOWN_ASCENT && countdownTimer > 0.35f) {
            int sec = static_cast<int>(std::ceil(countdownTimer));
            if (sec != lastCountSec) { lastCountSec = sec; SfxTone(660.0f, 0.12f, 'q', 0.05f); }
        }
        bool engines = false;
        if (stage == FlightStage::COUNTDOWN_ASCENT) engines = (countdownTimer <= 0.35f && fuelRemainingTons > 0.0f);
        else if (stage == FlightStage::TLI_BURN)    engines = tliBurning;
        else if (stage == FlightStage::LUNAR_DESCENT)
            engines = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE)) && landerFuelPct > 0.0f;
        rumbleTimer -= dt;
        if (engines && rumbleTimer <= 0.0f) {
            bool liftoff = (stage == FlightStage::COUNTDOWN_ASCENT && altitudeKm < 12.0f);
            SfxTone(static_cast<float>(GetRandomValue(55, 85)), 0.2f, 'n', liftoff ? 0.07f : 0.035f);
            rumbleTimer = 0.08f;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !(stage == FlightStage::DEBRIEF && endingTimer >= 0.0f)) {
        Rectangle btnMenu   = { 20.0f, 14.0f, 100.0f, 34.0f };
        Rectangle btnHangar = { 130.0f, 14.0f, 175.0f, 34.0f };
        if (CheckCollisionPointRec(mouse, btnMenu))   return GameState::MENU;
        if (CheckCollisionPointRec(mouse, btnHangar)) return GameState::HANGAR;
    }

    // ==================== 1. CUENTA ATRÁS Y ASCENSO ====================
    if (stage == FlightStage::COUNTDOWN_ASCENT) {
        if (IsKeyPressed(KEY_P)) autoPilot = !autoPilot;
        if (IsKeyPressed(KEY_SPACE)) timeWarp = (timeWarp == 1) ? 2 : 1;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Rectangle btnWarp  = { 315.0f, 14.0f, 125.0f, 34.0f };
            Rectangle btnPilot = { 450.0f, 14.0f, 175.0f, 34.0f };
            if (CheckCollisionPointRec(mouse, btnWarp))  timeWarp = (timeWarp == 1) ? 2 : 1;
            if (CheckCollisionPointRec(mouse, btnPilot)) autoPilot = !autoPilot;
        }

        float simDt = dt * static_cast<float>(timeWarp);

        if (countdownTimer > 0.0f) {
            countdownTimer = std::max(0.0f, countdownTimer - dt);
            return GameState::FLIGHT;
        }

        missionClock += simDt;

        // Controles de acelerador e inclinacion (Gravity Turn)
        if (autoPilot) {
            throttle = 1.0f;
            float targetPitch = std::clamp(90.0f - (altitudeKm / 160.0f) * 78.0f, 12.0f, 90.0f);
            pitchDeg += (targetPitch - pitchDeg) * std::min(1.0f, simDt * 2.0f);
        } else {
            if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))   throttle = std::min(1.0f, throttle + 0.7f * dt);
            if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) throttle = std::max(0.15f, throttle - 0.7f * dt);
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) pitchDeg = std::min(90.0f, pitchDeg + 28.0f * dt);
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) pitchDeg = std::max(10.0f, pitchDeg - 28.0f * dt);

            // Giro gravitatorio suave automatico si el jugador no toca A/D
            if (!IsKeyDown(KEY_LEFT) && !IsKeyDown(KEY_RIGHT) && !IsKeyDown(KEY_A) && !IsKeyDown(KEY_D)) {
                float naturalPitch = std::clamp(90.0f - (altitudeKm / 170.0f) * 75.0f, 15.0f, 90.0f);
                pitchDeg += (naturalPitch - pitchDeg) * std::min(1.0f, simDt * 0.55f);
            }
        }

        // Comprobacion de Empuje-Peso en rampa (TWR)
        float effectiveTwr = data.rocket.twr * throttle;
        if (altitudeKm <= 0.01f && effectiveTwr < 1.02f) {
            padFailTimer += simDt;
            if (padFailTimer >= 2.2f) {
                FinishMission(data, false, "F",
                    "FALLO EN RAMPA · EMPUJE INSUFICIENTE (TWR < 1,02)",
                    "PAD FAILURE · INSUFFICIENT LIFTOFF THRUST (TWR < 1.02)",
                    "Los motores rugen en la plataforma pero el cohete pesa demasiado para despegar. Vuelve al Hangar y equipa motores más potentes o propulsores laterales.",
                    "The engines roar on the pad, but the rocket is too heavy to lift off. Return to the Hangar and install stronger engines or side boosters.");
            }
            return GameState::FLIGHT;
        }

        // Separacion automatica de boosters al alcanzar 58 km
        if (boostersAttached && altitudeKm >= 58.0f) {
            boostersAttached = false;
            leftBooster  = { true, -46.0f, 0.0f, -55.0f, 95.0f, -8.0f };
            rightBooster = { true,  46.0f, 0.0f,  55.0f, 95.0f,  8.0f };
        }

        if (leftBooster.active) {
            leftBooster.offsetX += leftBooster.velX * simDt;
            leftBooster.offsetY += leftBooster.velY * simDt;
            leftBooster.velY += 140.0f * simDt;
            leftBooster.rotDeg -= 32.0f * simDt;
        }
        if (rightBooster.active) {
            rightBooster.offsetX += rightBooster.velX * simDt;
            rightBooster.offsetY += rightBooster.velY * simDt;
            rightBooster.velY += 140.0f * simDt;
            rightBooster.rotDeg += 32.0f * simDt;
        }

        // Fisica de ascenso rapido hacia orbita baja (180 km)
        float netTwr = std::max(0.15f, effectiveTwr - 0.85f);
        float rad = pitchDeg * (3.14159265f / 180.0f);
        velocityKms += netTwr * 0.62f * simDt;
        altitudeKm  += (0.8f + velocityKms * 2.4f) * std::sin(rad) * simDt * 3.8f;

        float burnRate = (data.rocket.maxFuelTons * 0.42f) / 16.0f;
        fuelRemainingTons = std::max(data.rocket.maxFuelTons * 0.45f, fuelRemainingTons - burnRate * throttle * simDt);

        if (altitudeKm >= 180.0f) {
            altitudeKm = 185.0f;
            velocityKms = std::max(velocityKms, 7.8f);
            stage = FlightStage::TLI_BURN;
            tliCursor = 0.12f;
            tliProgress = 0.0f;
        }
    }
    // ==================== 2. INYECCIÓN TRANSLUNAR (TIRACHINAS) ====================
    else if (stage == FlightStage::TLI_BURN) {
        tliClock += dt;
        float vmax = SlingMaxSpeed(data.rocket.deltaV);
        int rocks = std::clamp(data.difficulty, 0, 2) + 1;

        // Sin combustible: la nave sigue a la deriva unos segundos antes del informe
        if (driftTimer >= 0.0f) {
            float before = driftTimer;
            driftTimer += dt;
            tliBurning = false;
            simAccum += dt;
            while (simAccum >= kStep) {
                simAccum -= kStep;
                if (std::hypot(shipPos.x - kEarth.x, shipPos.y - kEarth.y) > 150.0f) GravStep(shipPos, shipVel, kStep);
            }
            shipAngle -= 150.0f * dt;   // dando vueltas
            if (static_cast<int>(before / 0.7f) != static_cast<int>(driftTimer / 0.7f)) SfxTone(520.0f, 0.16f, 'q', 0.05f, 0.7f);
            if (driftTimer >= 4.6f) {
                driftTimer = -1.0f;
                FinishMission(data, false, "C",
                    "SIN COMBUSTIBLE CAMINO DE LA LUNA",
                    "OUT OF FUEL ON THE WAY TO THE MOON",
                    TextFormat("El cohete tenía %.0f m/s de Delta-V y hacen falta 5.800 para llegar a la Luna, así que el motor se apagó a medio camino. En el hangar, un tanque más grande o un motor más eficiente lo arreglan.", data.rocket.deltaV),
                    TextFormat("The rocket had %.0f m/s of Delta-V and the Moon needs 5,800, so the engine cut out halfway. A bigger tank or a more efficient engine in the hangar will fix it.", data.rocket.deltaV));
            }
            return GameState::FLIGHT;
        }

        auto miss = [&](const char* es, const char* en) {
            sling = SlingState::MISS;
            slingTimer = 1.9f;
            missEs = es;
            missEn = en;
            tliBurning = false;
            SfxTone(200.0f, 0.3f, 'w', 0.06f, 0.5f);
        };

        if (sling == SlingState::AIM) {
            shipPos = kStart;
            tliBurning = false;
            velocityKms = 7.8f;
            if (!dragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                std::hypot(mouse.x - kStart.x, mouse.y - kStart.y) < 120.0f) {
                dragging = true;
            }
            if (dragging) {
                dragPos = mouse;
                float dx = kStart.x - mouse.x, dy = kStart.y - mouse.y;
                float len = std::hypot(dx, dy);
                if (len > 6.0f) shipAngle = std::atan2(-dy, dx) * 57.29578f;
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    dragging = false;
                    if (len > 28.0f) {
                        float power = std::min(1.0f, len / kMaxDrag);
                        shipVel = { dx / len * vmax * power, dy / len * vmax * power };
                        sling = SlingState::FLY;
                        flyTime = 0.0f;
                        simAccum = 0.0f;
                        trail.clear();
                        --shotsLeft;
                        lowFuelShot = (data.rocket.deltaV < 5800.0f);
                        SfxTone(180.0f, 0.5f, 'n', 0.08f, 2.4f);
                    } else {
                        shipAngle = 18.0f;
                    }
                }
            }
        } else if (sling == SlingState::FLY) {
            tliBurning = (flyTime < 0.7f);
            simAccum += dt;
            while (simAccum >= kStep && sling == SlingState::FLY) {
                simAccum -= kStep;
                flyTime += kStep;
                GravStep(shipPos, shipVel, kStep);

                if (std::hypot(shipPos.x - kMoon.x, shipPos.y - kMoon.y) < kCapture && !lowFuelShot) {
                    sling = SlingState::CAPTURED;
                    slingTimer = 1.5f;
                    tliProgress = 1.0f;
                    tliBurning = false;
                    SfxTone(523.0f, 0.12f); SfxTone(659.0f, 0.2f, 'q', 0.05f); SfxTone(784.0f, 0.32f, 'q', 0.05f);
                    break;
                }
                if (lowFuelShot) {
                    // con poco Delta-V el motor se apaga poco después de salir
                    if (flyTime > 1.3f) { driftTimer = 0.0f; SfxTone(140.0f, 0.5f, 'w', 0.07f, 0.4f); break; }
                    continue;
                }
                bool hitRock = false;
                for (int k = 0; k < rocks; ++k) {
                    Vector2 rp = RockPos(k, tliClock);
                    if (std::hypot(shipPos.x - rp.x, shipPos.y - rp.y) < RockRadius(k) + 14.0f) hitRock = true;
                }
                if (hitRock) { miss("¡Asteroide! Tiro perdido.", "Asteroid! Shot lost."); break; }
                if (flyTime > 0.3f && std::hypot(shipPos.x - kEarth.x, shipPos.y - kEarth.y) < 150.0f) {
                    miss("La gravedad de la Tierra te ha traído de vuelta.", "Earth's gravity pulled you back."); break;
                }
                if (shipPos.x < -80.0f || shipPos.x > SCREEN_WIDTH + 80.0f || shipPos.y < -260.0f || shipPos.y > SCREEN_HEIGHT + 80.0f) {
                    miss("Te has pasado de largo.", "You overshot into deep space."); break;
                }
                if (flyTime > 9.0f) { miss("Te has quedado dando vueltas.", "You ended up going in circles."); break; }
            }
            float speed = std::hypot(shipVel.x, shipVel.y);
            if (speed > 1.0f) shipAngle = std::atan2(-shipVel.y, shipVel.x) * 57.29578f;
            velocityKms = 7.8f + 3.1f * std::min(1.0f, speed / kNeed);
            if (trail.empty() || std::hypot(trail.back().x - shipPos.x, trail.back().y - shipPos.y) > 16.0f) {
                trail.push_back(shipPos);
                if (trail.size() > 400) trail.erase(trail.begin());
            }
        } else if (sling == SlingState::MISS) {
            slingTimer -= dt;
            if (slingTimer <= 0.0f) {
                if (shotsLeft > 0) {
                    sling = SlingState::AIM;
                    shipPos = kStart;
                    shipAngle = 18.0f;
                    trail.clear();
                } else {
                    FinishMission(data, false, "C",
                        "LA NAVE NO ENCONTRÓ EL CAMINO A LA LUNA",
                        "THE SHIP DID NOT FIND ITS WAY TO THE MOON",
                        "Se acabaron los intentos de inyección translunar. Fíjate en los puntos de la trayectoria, espera a que los asteroides dejen hueco y deja que la gravedad curve el tiro hasta el anillo de la Luna.",
                        "You ran out of translunar injection attempts. Watch the trajectory dots, wait for a gap between the asteroids and let gravity bend the shot into the ring around the Moon.");
                }
            }
        } else {   // CAPTURED: un momento de celebración y empieza el viaje
            slingTimer -= dt;
            if (slingTimer <= 0.0f) {
                // Viaje a la Luna: tres problemas que se resuelven con lo que haya en la mochila.
                // RunIncidents dibuja sus propias pantallas y vuelve cuando terminan.
                IncidentsResult trip = RunIncidents(data);
                if (trip.windowClosed) return GameState::FLIGHT;
                if (!trip.passed) {
                    FinishMission(data, false, "C",
                        "ESTA VEZ NO LLEGAMOS A LA LUNA",
                        "WE DID NOT REACH THE MOON THIS TIME",
                        trip.detailEs, trip.detailEn);
                    return GameState::FLIGHT;
                }
                stage = FlightStage::LUNAR_DESCENT;
                landerAltMeters = 1200.0f;
                landerVelY = -24.0f;
                landerVelX = 0.0f;
                landerFuelPct = 100.0f;
            }
        }
    }
    // ==================== 3. DESCENSO Y ALUNIZAJE POLO SUR ====================
    else if (stage == FlightStage::LUNAR_DESCENT) {
        bool thrustUp = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE)) && (landerFuelPct > 0.0f);
        bool thrustLeft = (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) && (landerFuelPct > 0.0f);
        bool thrustRight = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) && (landerFuelPct > 0.0f);

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Rectangle btnRetro = { 490.0f, 628.0f, 300.0f, 52.0f };
            if (CheckCollisionPointRec(mouse, btnRetro) && landerFuelPct > 0.0f) thrustUp = true;
        }

        const float lunarGravity = -1.62f; // Gravedad real de la Luna (1.62 m/s^2)
        float accelY = lunarGravity * 4.5f;
        float accelX = 0.0f;

        if (thrustUp) {
            accelY += 16.5f;
            landerFuelPct = std::max(0.0f, landerFuelPct - 8.5f * dt);
        }
        if (thrustLeft) {
            accelX -= 38.0f;
            landerFuelPct = std::max(0.0f, landerFuelPct - 2.2f * dt);
        }
        if (thrustRight) {
            accelX += 38.0f;
            landerFuelPct = std::max(0.0f, landerFuelPct - 2.2f * dt);
        }

        landerVelY += accelY * dt;
        landerVelX += accelX * dt;
        landerVelX *= (1.0f - 0.35f * dt);

        landerAltMeters += landerVelY * dt * 4.2f;
        landerPosX += landerVelX * dt;
        landerPosX = std::clamp(landerPosX, -460.0f, 460.0f);

        if (landerAltMeters <= 0.0f) {
            landerAltMeters = 0.0f;
            touchdownSpeed = std::fabs(landerVelY);
            bool onPad = (std::fabs(landerPosX) <= 95.0f);
            bool softTouch = (touchdownSpeed <= 6.2f);

            if (onPad && softTouch) {
                std::string g = (touchdownSpeed <= 3.0f && std::fabs(landerPosX) <= 35.0f) ? "S" : "A";
                FinishMission(data, true, g,
                    "¡ALUNIZAJE EXITOSO EN EL POLO SUR LUNAR!",
                    "SUCCESSFUL TOUCHDOWN AT THE LUNAR SOUTH POLE!",
                    TextFormat("El módulo %s tocó suelo lunar a %.1f m/s dentro de la zona segura LZ-Artemis. ¡Misión histórica cumplida!", data.rocket.lander.name.c_str(), touchdownSpeed),
                    TextFormat("Lander %s touched down at %.1f m/s inside the LZ-Artemis safe zone. Historic lunar mission accomplished!", data.rocket.lander.name.c_str(), touchdownSpeed));
            } else if (!softTouch) {
                FinishMission(data, false, "D",
                    "IMPACTO BRUSCO EN SUPERFICIE LUNAR (VELOCIDAD EXCESIVA)",
                    "HARD LUNAR IMPACT (EXCESSIVE VERTICAL VELOCITY)",
                    TextFormat("Tocaste superficie a %.1f m/s (máximo seguro: 6,2 m/s). Usa los retropropulsores (W / ESPACIO) antes de llegar al suelo.", touchdownSpeed),
                    TextFormat("Touchdown occurred at %.1f m/s (safe limit: 6.2 m/s). Fire retro-thrusters (W / SPACE) before reaching the ground.", touchdownSpeed));
            } else {
                FinishMission(data, false, "D",
                    "ALUNIZAJE FUERA DE ZONA · CRÁTER ESCARPADO",
                    "OFF-TARGET LANDING · STEEP CRATER SLOPE",
                    "El módulo descendió suave pero cayó fuera de la plataforma iluminada en terreno inclinado. Usa A / D para centrarte sobre el marcador verde.",
                    "Descent speed was safe, but the lander came down outside the lit pad on steep terrain. Use A / D to center over the green marker.");
            }
        }
    }
    // ==================== 4. INFORME FINAL ====================
    else if (stage == FlightStage::DEBRIEF && endingTimer >= 0.0f) {
        // Escena final: saltos, bandera, fanfarria y fuegos artificiales. Un clic pasa al informe.
        float before = endingTimer;
        endingTimer += dt;
        auto crossed = [&](float t0) { return before < t0 && endingTimer >= t0; };
        if (endingTimer > kEndWalk0 && endingTimer < kEndWalk1) {
            if (static_cast<int>((before - kEndWalk0) / kEndHop) != static_cast<int>((endingTimer - kEndWalk0) / kEndHop) || crossed(kEndWalk0 + 0.001f))
                SfxTone(240.0f, 0.14f, 's', 0.05f, 1.9f);
        }
        if (crossed(kEndWalk1)) SfxTone(120.0f, 0.2f, 'n', 0.07f);                 // la bandera se clava
        const float notes[6][3] = { { 0.0f, 523.0f, 0.16f }, { 0.16f, 659.0f, 0.16f }, { 0.32f, 784.0f, 0.16f },
                                    { 0.48f, 1047.0f, 0.3f }, { 0.82f, 784.0f, 0.14f }, { 0.98f, 1047.0f, 0.7f } };
        for (const auto& n : notes) if (crossed(kEndParty + n[0])) SfxTone(n[1], n[2], 'q', 0.06f);
        if (crossed(kEndParty + 0.3f)) VoiceSay("wuhuuu viva bieeen llegamos a la luna", 430.0f, 14);
        if (endingTimer > kEndParty) {
            if (static_cast<int>((before - kEndParty) / 0.7f) != static_cast<int>((endingTimer - kEndParty) / 0.7f))
                SfxTone(static_cast<float>(GetRandomValue(180, 420)), 0.18f, 'n', 0.035f);       // fuegos artificiales
            if (static_cast<int>((before - kEndParty) / 1.1f) != static_cast<int>((endingTimer - kEndParty) / 1.1f))
                SfxTone(300.0f, 0.14f, 's', 0.04f, 2.0f);                                          // saltos de alegría
        }
        if (endingTimer > 2.5f && (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
            endingTimer = -1.0f;
            VoiceStop();
        }
    }
    else if (stage == FlightStage::DEBRIEF) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Rectangle btnRetry  = { 245.0f, 520.0f, 240.0f, 48.0f };
            Rectangle btnHangar = { 515.0f, 520.0f, 250.0f, 48.0f };
            Rectangle btnMenu   = { 795.0f, 520.0f, 240.0f, 48.0f };

            if (CheckCollisionPointRec(mouse, btnRetry))  OnEnter(data);
            if (CheckCollisionPointRec(mouse, btnHangar)) return GameState::HANGAR;
            if (CheckCollisionPointRec(mouse, btnMenu))   return GameState::MENU;
        }
    }

    return GameState::FLIGHT;
}

void MissionPhase::DrawModularRocket(const SharedMissionData& data, float centerX, float bottomY,
                                     float scale, float angleDeg, bool firingMain, bool firingBoosters) const {
    int capIdx   = std::clamp(data.eqCapsuleIdx, 0, 5);
    int lanIdx   = std::clamp(data.eqLanderIdx - 6, 0, 5);
    int tnkIdx   = std::clamp(data.eqTankIdx - 12, 0, 5);
    int engIdx   = std::clamp(data.eqEngineIdx - 18, 0, 5);
    int boostIdx = (data.eqBoosterIdx >= 25) ? std::clamp(data.eqBoosterIdx - 25, 0, 4) : -1;

    float rad = (90.0f - angleDeg) * (3.14159265f / 180.0f);
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    auto rotPoint = [&](float localX, float localY) -> Vector2 {
        return {
            centerX + (localX * cosA - localY * sinA) * scale,
            bottomY + (localX * sinA + localY * cosA) * scale
        };
    };

    // Cohete terminado: uno de los tres modelos, según la calidad de las piezas del hangar
    Texture2D finalTex = texFinalRocket[RocketModelFromParts(data) - 1];
    bool useFinal = (finalTex.id > 0);

    // Fuego pixel-art por capas (rojo oscuro, naranja, dorado, núcleo blanco) con chispas, una llama por tobera
    if (firingMain && useFinal) {
        float t = static_cast<float>(GetTime());
        float rot = 90.0f - angleDeg;
        auto px = [&](float lx, float ly, float w, float h, Color c) {
            Vector2 p = rotPoint(lx, ly);
            DrawRectanglePro({ p.x, p.y, w * scale, h * scale }, { w * scale * 0.5f, 0.0f }, rot, c);
        };
        const float cell = 6.0f;
        for (int nz = -1; nz <= 1; nz += 2) {
            float nx = nz * 21.0f;
            float len = 118.0f + 16.0f * std::sin(t * 23.0f + nz) + 9.0f * std::sin(t * 37.0f + 1.0f) + 6.0f * std::sin(t * 61.0f + 2.0f * nz);
            int rows = static_cast<int>(len / cell);
            for (int i = 0; i < rows; ++i) {
                float u = static_cast<float>(i) / rows;
                float wob = std::sin(t * 18.0f - i * 0.55f + nz) * u * 6.0f;
                float half = std::max(2.0f, std::pow(1.0f - u, 0.75f) * 17.0f + std::sin(t * 31.0f - i * 0.9f) * 3.0f * u);
                float ly = -6.0f + i * cell;
                px(nx + wob, ly, 2.0f * half + 8.0f, cell + 1.0f, Color{ 158, 42, 28, 255 });
                px(nx + wob, ly, 2.0f * half, cell + 1.0f, Color{ 232, 99, 44, 255 });
                if (u < 0.72f) px(nx + wob, ly, 2.0f * half * 0.62f, cell + 1.0f, Color{ 242, 193, 78, 255 });
                if (u < 0.42f) px(nx + wob, ly, 2.0f * half * 0.34f, cell + 1.0f, Color{ 255, 243, 176, 255 });
                if (u < 0.16f) px(nx + wob, ly, 2.0f * half * 0.20f, cell + 1.0f, WHITE);
            }
            for (int k = 0; k < 9; ++k) {   // chispas que se alejan y se apagan
                float ph = std::fmod(t * (1.5f + (k % 4) * 0.25f) + k * 0.618f + nz * 0.3f, 1.0f);
                float sz = (1.0f - ph) * 7.0f + 3.0f;
                Color c = ph < 0.35f ? Color{ 255, 243, 176, 255 } : (ph < 0.7f ? Color{ 242, 193, 78, 255 } : Color{ 232, 99, 44, 255 });
                c.a = static_cast<unsigned char>(255.0f * (1.0f - ph));
                px(nx + (Hash(k + 7 * nz) - 0.5f) * 46.0f * ph, len * (0.55f + ph * 1.1f), sz, sz, c);
            }
        }
    }

    // Llamas inferiores si los motores estan encendidos
    if (firingMain && !useFinal) {
        float flicker = 75.0f + static_cast<float>(GetRandomValue(-14, 22));
        if (texFlameMain1.id > 0) {
            Texture2D fTex = (GetRandomValue(0, 1) == 0 || texFlameMain2.id == 0) ? texFlameMain1 : texFlameMain2;
            float fw = 110.0f * scale;
            float fh = 155.0f * scale;
            DrawTexturePro(fTex, { 0, 0, (float)fTex.width, (float)fTex.height },
                           { centerX, bottomY, fw, fh }, { fw * 0.5f, 0.0f }, 90.0f - angleDeg, WHITE);
        } else {
            Vector2 pL = rotPoint(-22.0f, 0.0f);
            Vector2 pR = rotPoint( 22.0f, 0.0f);
            Vector2 pTip = rotPoint(0.0f, flicker);
            DrawTriangle(pL, pTip, pR, ORANGE);

            Vector2 pL2 = rotPoint(-11.0f, 0.0f);
            Vector2 pR2 = rotPoint( 11.0f, 0.0f);
            Vector2 pTip2 = rotPoint(0.0f, flicker * 0.62f);
            DrawTriangle(pL2, pTip2, pR2, WHITE);
        }
    }

    if (useFinal) {
        // El sprite ya trae el cohete entero (sin boosters aparte): se dibuja girado sobre su base
        const float kRocketH = 250.0f;
        float h = kRocketH * scale;
        float w = h * static_cast<float>(finalTex.width) / static_cast<float>(finalTex.height);
        DrawTexturePro(finalTex, { 0, 0, (float)finalTex.width, (float)finalTex.height },
                       { centerX, bottomY, w, h }, { w * 0.5f, h }, 90.0f - angleDeg, WHITE);
        return;
    }

    if (firingBoosters && boostIdx >= 0) {
        float bFlicker = 62.0f + static_cast<float>(GetRandomValue(-10, 18));
        for (int side = -1; side <= 1; side += 2) {
            float bx = side * 48.0f;
            Vector2 bL = rotPoint(bx - 10.0f, -6.0f);
            Vector2 bR = rotPoint(bx + 10.0f, -6.0f);
            Vector2 bTip = rotPoint(bx, bFlicker);
            DrawTriangle(bL, bTip, bR, GOLD);
        }
    }

    // Si existen sprites PNG modulares dibujados por el equipo, se apilan centrados
    bool hasAnyModularPng = (texCapsules[capIdx].id > 0 || texTanks[tnkIdx].id > 0 || texEngines[engIdx].id > 0);
    if (hasAnyModularPng) {
        // Cada sprite conserva su proporción (ancho fijo, altura según la imagen) y se apila
        // a lo largo del eje del cohete, también cuando está inclinado.
        const float kEngW = 58.0f, kTankW = 66.0f, kCapW = 74.0f, kBoostW = 64.0f, kBoostOffsetX = 59.0f;
        float rotDeg = 90.0f - angleDeg;
        auto partH = [](Texture2D t, float w) {
            return (t.id > 0) ? w * static_cast<float>(t.height) / static_cast<float>(t.width) : 0.0f;
        };
        auto drawPartTex = [&](Texture2D t, float w, float localX, float localBottom, bool mirror) {
            if (t.id <= 0) return;
            float h = partH(t, w);
            Vector2 p = rotPoint(localX, -localBottom);
            DrawTexturePro(t, { 0, 0, (mirror ? -1.0f : 1.0f) * (float)t.width, (float)t.height },
                           { p.x, p.y, w * scale, h * scale }, { w * scale * 0.5f, h * scale }, rotDeg, WHITE);
        };

        float engH = partH(texEngines[engIdx], kEngW);
        float tnkH = partH(texTanks[tnkIdx], kTankW);
        float lanH = partH(texLanders[lanIdx], kTankW);

        if (boostIdx >= 0 && texBoosters[boostIdx].id > 0) {
            drawPartTex(texBoosters[boostIdx], kBoostW, -kBoostOffsetX, engH - 14.0f, false);
            drawPartTex(texBoosters[boostIdx], kBoostW,  kBoostOffsetX, engH - 14.0f, true);
        }
        drawPartTex(texEngines[engIdx],  kEngW,  0.0f, 0.0f, false);
        drawPartTex(texTanks[tnkIdx],    kTankW, 0.0f, engH, false);
        drawPartTex(texLanders[lanIdx],  kTankW, 0.0f, engH + tnkH, false);
        drawPartTex(texCapsules[capIdx], kCapW,  0.0f, engH + tnkH + lanH, false);
        return;
    }

    // Renderizado tecnico modular NASA (adaptado a las piezas exactas compradas)
    float tankW = 46.0f + tnkIdx * 5.5f;   // 46..73 px segun el tanque elegido
    float tankH = 128.0f + tnkIdx * 9.0f;  // 128..173 px
    float engH  = 34.0f;
    float lanH  = 52.0f;
    float capH  = 52.0f;

    auto drawRotatedBox = [&](float localYTop, float localYBot, float width, Color fill, Color border) {
        float hw = width * 0.5f;
        Vector2 tl = rotPoint(-hw, localYTop);
        Vector2 tr = rotPoint( hw, localYTop);
        Vector2 br = rotPoint( hw, localYBot);
        Vector2 bl = rotPoint(-hw, localYBot);
        DrawTriangle(tl, bl, br, fill);
        DrawTriangle(tl, br, tr, fill);
        DrawLineEx(tl, tr, 1.5f, border);
        DrawLineEx(tr, br, 1.5f, border);
        DrawLineEx(br, bl, 1.5f, border);
        DrawLineEx(bl, tl, 1.5f, border);
    };

    // 1. Motor principal (toberas)
    int nozzleCount = std::min(5, 1 + engIdx);
    float spread = (tankW * 0.72f) / std::max(1, nozzleCount);
    for (int n = 0; n < nozzleCount; ++n) {
        float nx = (nozzleCount == 1) ? 0.0f : (-tankW * 0.32f + n * spread);
        Vector2 nTopL = rotPoint(nx - 7.0f, -engH);
        Vector2 nTopR = rotPoint(nx + 7.0f, -engH);
        Vector2 nBotL = rotPoint(nx - 11.0f, 0.0f);
        Vector2 nBotR = rotPoint(nx + 11.0f, 0.0f);
        DrawTriangle(nTopL, nBotL, nBotR, Color{ 55, 62, 74, 255 });
        DrawTriangle(nTopL, nBotR, nTopR, Color{ 85, 94, 110, 255 });
    }

    // 2. Tanque de combustible (naranja criogenico para SLS/Super-H2, blanco NASA para Saturno/Redstone)
    float tankBot = -engH;
    float tankTop = tankBot - tankH;
    Color tankColor = (tnkIdx == 3 || tnkIdx == 5) ? Color{ 218, 108, 38, 255 } : Color{ 238, 242, 248, 255 };
    drawRotatedBox(tankTop, tankBot, tankW, tankColor, Color{ 35, 45, 65, 255 });
    drawRotatedBox(tankBot - 16.0f, tankBot, tankW, Color{ 25, 30, 42, 255 }, BLACK);
    drawRotatedBox(tankTop, tankTop + 14.0f, tankW, Color{ 25, 30, 42, 255 }, BLACK);

    // 3. Modulo Lunar / Carena
    float lanBot = tankTop;
    float lanTop = lanBot - lanH;
    drawRotatedBox(lanTop, lanBot, tankW * 0.86f, Color{ 225, 230, 238, 255 }, Color{ 40, 50, 70, 255 });
    drawRotatedBox(lanTop + 14.0f, lanBot - 12.0f, tankW * 0.56f, Color{ 212, 168, 46, 255 }, DARKBROWN);

    // 4. Capsula tripulada + Torre de Escape (LES)
    float capBot = lanTop;
    float capTop = capBot - capH;
    Color capColor = (capIdx <= 1) ? Color{ 62, 68, 78, 255 } : Color{ 242, 245, 250, 255 };
    Vector2 cBL = rotPoint(-tankW * 0.42f, capBot);
    Vector2 cBR = rotPoint( tankW * 0.42f, capBot);
    Vector2 cTip = rotPoint(0.0f, capTop);
    DrawTriangle(cBL, cBR, cTip, capColor);
    DrawLineEx(cBL, cTip, 2.0f, DARKGRAY);
    DrawLineEx(cBR, cTip, 2.0f, DARKGRAY);

    // Torre LES
    Vector2 lesBase = rotPoint(0.0f, capTop);
    Vector2 lesTip  = rotPoint(0.0f, capTop - 28.0f);
    DrawLineEx(lesBase, lesTip, 3.0f, RED);

    // 5. Propulsores laterales (si estan acoplados)
    if (boostIdx >= 0) {
        float bW = 16.0f + boostIdx * 2.0f;
        float bH = tankH * 0.88f;
        Color bCol = (boostIdx == 1 || boostIdx == 4) ? Color{ 48, 54, 66, 255 } : Color{ 245, 247, 252, 255 };
        for (int side = -1; side <= 1; side += 2) {
            float bx = side * (tankW * 0.5f + bW * 0.55f);
            drawRotatedBox(tankBot - bH, tankBot + 4.0f, bW, bCol, BLACK);
            Vector2 noseL = rotPoint(bx - bW * 0.5f, tankBot - bH);
            Vector2 noseR = rotPoint(bx + bW * 0.5f, tankBot - bH);
            Vector2 noseT = rotPoint(bx, tankBot - bH - 22.0f);
            DrawTriangle(noseL, noseR, noseT, RED);
        }
    }
}

void MissionPhase::DrawAscentScene(const SharedMissionData& data) const {
    float spaceRatio = std::clamp(altitudeKm / 130.0f, 0.0f, 1.0f);
    float tNow = static_cast<float>(GetTime());

    // Cielo en franjas (como el cielo pixel-art del espacio): de azul de día a noche.
    // Con la imagen de la plataforma, el cielo de día es del mismo color que el de la imagen.
    bool hasLaunchArt = (texLaunchBg.id > 0);
    {
        const Color artSky = { 99, 193, 201, 255 };
        const Color dayTop = hasLaunchArt ? artSky : Color{ 46, 92, 178, 255 };
        const Color dayBot = hasLaunchArt ? artSky : Color{ 142, 200, 240, 255 };
        const Color nightTop = { 2, 2, 22, 255 },  nightBot = { 4, 4, 80, 255 };
        const int bands = 9;
        for (int b = 0; b < bands; ++b) {
            float v = (b + 0.5f) / bands;
            Color c = Mix(Mix(dayTop, dayBot, v), Mix(nightTop, nightBot, v), spaceRatio);
            DrawRectangle(0, b * SCREEN_HEIGHT / bands, SCREEN_WIDTH, SCREEN_HEIGHT / bands + 1, c);
        }
    }

    // El cielo estrellado aparece al subir hacia la estratosfera
    if (spaceRatio > 0.25f) {
        DrawSky((spaceRatio - 0.25f) / 0.75f);
    }

    // Nubes en bloques que se quedan atrás al subir
    for (int i = 0; i < 7; ++i) {
        float cy = -80.0f - i * 210.0f + altitudeKm * 62.0f;
        if (cy < -120.0f || cy > SCREEN_HEIGHT + 40.0f) continue;
        float cx = std::fmod(90.0f + i * 397.0f + tNow * 6.0f, SCREEN_WIDTH + 320.0f) - 160.0f;
        float w = 180.0f + Hash(i) * 120.0f;
        unsigned char a = static_cast<unsigned char>(235.0f * (1.0f - spaceRatio));
        PxRect(cx, cy + 16.0f, w, 28.0f, Color{ 211, 235, 239, a });
        PxRect(cx + 28.0f, cy, w * 0.5f, 20.0f, Color{ 255, 255, 255, a });
        PxRect(cx + w * 0.45f, cy + 8.0f, w * 0.4f, 16.0f, Color{ 255, 255, 255, a });
        PxRect(cx + 12.0f, cy + 40.0f, w - 28.0f, 8.0f, Color{ 182, 203, 208, a });
    }

    // Complejo de lanzamiento: la imagen del equipo (torre y plataforma) que se queda abajo al subir
    float groundOffset = altitudeKm * 38.0f;
    if (hasLaunchArt && groundOffset < 760.0f) {
        // Escala y posición para que la plataforma de la imagen quede justo bajo el cohete (x = 640, y = 608)
        const float artScale = 0.80f;
        float aw = texLaunchBg.width * artScale, ah = texLaunchBg.height * artScale;
        DrawTexturePro(texLaunchBg, { 0, 0, (float)texLaunchBg.width, (float)texLaunchBg.height },
                       { 640.0f - 840.0f * artScale, 608.0f - 770.0f * artScale + Snap(groundOffset), aw, ah }, { 0, 0 }, 0.0f, WHITE);
        // por debajo de la imagen, más hierba
        float below = 608.0f - 770.0f * artScale + Snap(groundOffset) + ah;
        if (below < SCREEN_HEIGHT) DrawRectangle(0, static_cast<int>(below) - 1, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 48, 155, 99, 255 });
    }
    if (groundOffset < 760.0f) {
        float padY = Snap(635.0f + groundOffset);
        if (!hasLaunchArt) {

        // Mar al fondo
        DrawRectangle(0, static_cast<int>(padY - 48), SCREEN_WIDTH, 48, Color{ 40, 88, 160, 255 });
        DrawRectangle(0, static_cast<int>(padY - 48), SCREEN_WIDTH, 4, Color{ 110, 170, 225, 255 });
        for (int i = 0; i < 14; ++i) {
            float wx = std::fmod(i * 97.0f + tNow * 14.0f, static_cast<float>(SCREEN_WIDTH));
            PxRect(wx, padY - 36.0f + (i % 3) * 10.0f, 28.0f, 4.0f, Color{ 84, 140, 205, 255 });
        }

        // Edificio de ensamblaje a lo lejos
        PxRect(996.0f, padY - 172.0f, 168.0f, 172.0f, kInk);
        PxRect(1000.0f, padY - 168.0f, 160.0f, 168.0f, Color{ 182, 203, 208, 255 });
        PxRect(1000.0f, padY - 168.0f, 160.0f, 12.0f, Color{ 211, 235, 239, 255 });
        PxRect(1128.0f, padY - 156.0f, 32.0f, 156.0f, Color{ 144, 161, 168, 255 });
        PxRect(1040.0f, padY - 120.0f, 44.0f, 120.0f, Color{ 65, 72, 90, 255 });
        PxRect(1016.0f, padY - 148.0f, 100.0f, 12.0f, Color{ 50, 80, 150, 255 });

        // Hierba
        DrawRectangle(0, static_cast<int>(padY), SCREEN_WIDTH, 240, Color{ 58, 125, 78, 255 });
        DrawRectangle(0, static_cast<int>(padY), SCREEN_WIDTH, 8, Color{ 88, 179, 104, 255 });
        DrawRectangle(0, static_cast<int>(padY - 4), SCREEN_WIDTH, 4, kInk);
        for (int i = 0; i < 46; ++i) {
            PxRect(Hash(i) * SCREEN_WIDTH, padY + 16.0f + Hash(i + 50) * 64.0f, 12.0f, 4.0f, Color{ 44, 100, 62, 255 });
        }
        // Camino de grava por el que llegó el cohete
        PxRect(700.0f, padY + 20.0f, 600.0f, 28.0f, Color{ 150, 142, 124, 255 });
        PxRect(700.0f, padY + 20.0f, 600.0f, 4.0f, Color{ 182, 174, 152, 255 });

        // Torre de servicio: celosía de acero con el mismo contorno oscuro que los sprites
        float towerTop = padY - 420.0f;
        PxRect(476.0f, towerTop - 4.0f, 64.0f, 396.0f, kInk);
        PxRect(480.0f, towerTop, 56.0f, 392.0f, Color{ 65, 72, 90, 255 });
        for (float ly = towerTop; ly < padY - 32.0f; ly += 36.0f) {
            PxRect(480.0f, ly, 56.0f, 4.0f, Color{ 144, 161, 168, 255 });
            bool flip = (static_cast<int>((ly - towerTop) / 36.0f) % 2) == 0;
            for (int d = 1; d < 8; ++d) {
                float dx = flip ? d * 6.0f : 48.0f - d * 6.0f;
                PxRect(482.0f + dx, ly + 2.0f + d * 4.0f, 4.0f, 4.0f, Color{ 104, 113, 122, 255 });
            }
        }
        PxRect(480.0f, towerTop, 8.0f, 392.0f, Color{ 104, 113, 122, 255 });
        PxRect(528.0f, towerTop, 8.0f, 392.0f, Color{ 104, 113, 122, 255 });
        // Pararrayos con baliza
        PxRect(500.0f, towerTop - 64.0f, 16.0f, 64.0f, kInk);
        PxRect(504.0f, towerTop - 60.0f, 8.0f, 60.0f, Color{ 182, 203, 208, 255 });
        if (std::fmod(tNow, 1.0f) < 0.5f) PxRect(500.0f, towerTop - 76.0f, 16.0f, 12.0f, Color{ 232, 60, 50, 255 });
        // Brazo de acceso: se retira justo antes del encendido
        float armExtend = (countdownTimer > 0.6f) ? 76.0f : 20.0f;
        PxRect(536.0f, padY - 236.0f, armExtend + 4.0f, 20.0f, kInk);
        PxRect(536.0f, padY - 232.0f, armExtend, 12.0f, Color{ 182, 203, 208, 255 });
        PxRect(536.0f, padY - 224.0f, armExtend, 4.0f, Color{ 144, 161, 168, 255 });

        // Plataforma de hormigón con franja de seguridad amarilla y negra (como el hangar)
        PxRect(436.0f, padY - 32.0f, 408.0f, 36.0f, kInk);
        PxRect(440.0f, padY - 28.0f, 400.0f, 28.0f, Color{ 144, 161, 168, 255 });
        PxRect(440.0f, padY - 28.0f, 400.0f, 8.0f, Color{ 182, 203, 208, 255 });
        for (int k = 0; k < 17; ++k) {
            PxRect(440.0f + k * 24.0f, padY - 12.0f, 12.0f, 8.0f, Color{ 242, 240, 19, 255 });
            PxRect(452.0f + k * 24.0f, padY - 12.0f, 12.0f, 8.0f, Color{ 16, 16, 20, 255 });
        }
        PxRect(592.0f, padY - 20.0f, 96.0f, 20.0f, Color{ 30, 28, 40, 255 });   // foso de llamas
        }

        // Encendido: resplandor en el foso y nubes de humo en bloques que se abren a los lados
        if (countdownTimer <= 0.4f) {
            float e = std::min(1.0f, (0.4f - countdownTimer + missionClock) * 0.8f);
            if (altitudeKm < 4.0f) {
                PxRect(580.0f, padY - 28.0f, 120.0f, 12.0f, Color{ 255, 243, 176, 255 });
                PxRect(560.0f, padY - 16.0f, 160.0f, 16.0f, Color{ 242, 193, 78, 255 });
            }
            const Color smoke[3] = { { 182, 203, 208, 255 }, { 211, 235, 239, 255 }, { 245, 250, 252, 255 } };
            for (int layer = 0; layer < 3; ++layer) {
                for (int i = layer; i < 54; i += 3) {
                    float side = (i % 2 == 0) ? 1.0f : -1.0f;
                    float d = (0.08f + Hash(i)) * e * 440.0f;
                    float sz = 24.0f + Hash(i + 9) * 60.0f * (0.4f + 0.6f * e);
                    float lift = Hash(i + 31) * 70.0f * e * (1.0f - d / 520.0f);
                    float bob = std::sin(tNow * 3.0f + i) * 5.0f;
                    PxRect(640.0f + side * d - sz * 0.5f, padY + 4.0f - sz - lift + bob, sz, sz, smoke[layer]);
                }
            }
        }
    }

    // Vibracion de camara durante el encendido
    bool ignited = (countdownTimer <= 0.35f);
    float shakeX = (ignited && altitudeKm < 35.0f) ? static_cast<float>(GetRandomValue(-3, 3)) : 0.0f;
    float shakeY = (ignited && altitudeKm < 35.0f) ? static_cast<float>(GetRandomValue(-2, 2)) : 0.0f;

    float rocketBaseY = 610.0f - std::min(195.0f, altitudeKm * 3.2f) + shakeY;
    DrawModularRocket(data, 640.0f + shakeX, rocketBaseY, 1.05f, pitchDeg, ignited, ignited && boostersAttached);

    // Boosters separandose
    bool finalRocketArt = (texFinalRocket[RocketModelFromParts(data) - 1].id > 0);
    if (leftBooster.active && !finalRocketArt) {
        DrawRectanglePro({ 640.0f + leftBooster.offsetX, rocketBaseY - 90.0f + leftBooster.offsetY, 16.0f, 120.0f },
                         { 8.0f, 60.0f }, leftBooster.rotDeg, WHITE);
    }
    if (rightBooster.active && !finalRocketArt) {
        DrawRectanglePro({ 640.0f + rightBooster.offsetX, rocketBaseY - 90.0f + rightBooster.offsetY, 16.0f, 120.0f },
                         { 8.0f, 60.0f }, rightBooster.rotDeg, WHITE);
    }

    // Cartel grande de cuenta atras rapida
    if (countdownTimer > 0.0f) {
        int sec = static_cast<int>(std::ceil(countdownTimer));
        const char* cdText = (countdownTimer > 0.35f)
            ? TextFormat("T - %d", sec)
            : Tr(data.lang, "¡IGNICIÓN · DESPEGUE!", "IGNITION · LIFTOFF!");
        DrawRectangle(450, 120, 380, 68, Color{ 12, 18, 34, 235 });
        DrawRectangleLinesEx({ 450.0f, 120.0f, 380.0f, 68.0f }, 2.5f, GOLD);
        float tw = MeasureUIText(cdText, 34.0f);
        DrawUIText(cdText, 640.0f - tw * 0.5f, 136.0f, 34.0f, countdownTimer > 0.35f ? GOLD : GREEN);
    }

    // Panel de controles de ascenso
    DrawRectangle(20, 80, 330, 195, Color{ 12, 18, 34, 235 });
    DrawRectangleLinesEx({ 20.0f, 80.0f, 330.0f, 195.0f }, 2.0f, SKYBLUE);
    DrawUIText(Tr(data.lang, "ETAPA 1 · ASCENSO ORBITAL", "STAGE 1 · ORBITAL ASCENT"), 36.0f, 92.0f, 18.0f, GOLD);
    DrawUIText(TextFormat("TWR: %.2f  |  Delta-V: %.0f m/s", data.rocket.twr, data.rocket.deltaV), 36.0f, 122.0f, 16.0f, WHITE);
    DrawUIText(TextFormat(Tr(data.lang, "Acelerador: %.0f%% (W / S)", "Throttle: %.0f%% (W / S)"), throttle * 100.0f), 36.0f, 150.0f, 16.0f, GREEN);
    DrawUIText(TextFormat(Tr(data.lang, "Inclinación: %.0f° (A / D)", "Pitch: %.0f° (A / D)"), pitchDeg), 36.0f, 178.0f, 16.0f, SKYBLUE);
    DrawUIText(Tr(data.lang, "Objetivo: Alcanzar 180 km de altitud", "Goal: Reach 180 km orbital altitude"), 36.0f, 212.0f, 15.0f, LIGHTGRAY);
    DrawUIText(Tr(data.lang, "P: Piloto Automático  |  ESPACIO: x2", "P: Auto-Pilot  |  SPACE: x2 Speed"), 36.0f, 240.0f, 14.0f, GOLD);
}

void MissionPhase::DrawTLIScene(const SharedMissionData& data) const {
    DrawSky(1.0f);
    Vector2 mouse = GetMousePosition();
    float tNow = static_cast<float>(GetTime());
    const Color cGold = { 242, 193, 78, 255 };
    const Color cDim  = { 201, 208, 240, 255 };

    // Tierra a la izquierda y Luna a la derecha
    if (texEarth.id > 0) {
        DrawSpriteCentered(texEarth, kEarth.x, kEarth.y, 300.0f);
    } else {
        DrawCircle(185, 390, 145, Color{ 32, 96, 188, 255 });
        DrawCircleLines(185, 390, 148, Color{ 110, 185, 255, 190 });
    }
    DrawUIText(Tr(data.lang, "TIERRA", "EARTH"), 150.0f, 556.0f, 20.0f, SKYBLUE);

    if (texMoon.id > 0) {
        DrawSpriteCentered(texMoon, kMoon.x, kMoon.y, 150.0f);
    } else {
        DrawCircle(1085, 245, 68, Color{ 212, 214, 222, 255 });
    }
    // Anillo de captura: el objetivo del tiro
    {
        bool got = (sling == SlingState::CAPTURED);
        float pulse = got ? 1.0f : 0.6f + 0.4f * std::sin(tNow * 4.0f);
        Color ring = got ? Color{ 88, 179, 104, 255 } : Color{ 242, 193, 78, static_cast<unsigned char>(120 + 120 * pulse) };
        for (int k = 0; k < 28; ++k) {
            float a = k * 6.2831853f / 28.0f + tNow * 0.5f;
            DrawRectangleRec({ kMoon.x + std::cos(a) * kCapture - 4.0f, kMoon.y + std::sin(a) * kCapture - 4.0f, 8.0f, 8.0f }, ring);
        }
    }
    DrawUIText(Tr(data.lang, "LUNA", "MOON"), 1060.0f, 356.0f, 20.0f, cGold);

    // Asteroides
    int rocks = std::clamp(data.difficulty, 0, 2) + 1;
    for (int k = 0; k < rocks; ++k) {
        Vector2 rp = RockPos(k, tliClock);
        float r = RockRadius(k), q = std::round(r / 4.0f);
        DrawRectangleRec({ rp.x - r - 3, rp.y - r + q - 3, 2 * r + 6, 2 * (r - q) + 6 }, kInk);
        DrawRectangleRec({ rp.x - r + q - 3, rp.y - r - 3, 2 * (r - q) + 6, 2 * r + 6 }, kInk);
        DrawRectangleRec({ rp.x - r, rp.y - r + q, 2 * r, 2 * (r - q) }, Color{ 141, 143, 156, 255 });
        DrawRectangleRec({ rp.x - r + q, rp.y - r, 2 * (r - q), 2 * r }, Color{ 141, 143, 156, 255 });
        DrawRectangleRec({ rp.x - r * 0.5f, rp.y - r * 0.4f, r * 0.45f, r * 0.4f }, Color{ 111, 113, 125, 255 });
        DrawRectangleRec({ rp.x + r * 0.15f, rp.y + r * 0.2f, r * 0.5f, r * 0.35f }, Color{ 111, 113, 125, 255 });
        DrawRectangleRec({ rp.x - r + q, rp.y - r + 2, r * 0.8f, 3 }, Color{ 185, 187, 200, 255 });
    }

    // Estela del tiro en curso
    for (size_t k = 0; k < trail.size(); ++k) {
        DrawRectangleRec({ trail[k].x - 3.0f, trail[k].y - 3.0f, 6.0f, 6.0f }, Color{ 127, 209, 255, 150 });
    }

    float vmax = SlingMaxSpeed(data.rocket.deltaV);
    Vector2 drawPos = shipPos;
    float power = 0.0f;

    if (sling == SlingState::AIM) {
        drawPos = kStart;
        if (dragging) {
            float dx = kStart.x - mouse.x, dy = kStart.y - mouse.y;
            float len = std::hypot(dx, dy);
            power = std::min(1.0f, len / kMaxDrag);
            if (len > 6.0f) {
                // el cohete se echa hacia atrás, como en un tirachinas
                drawPos = { kStart.x - dx / len * 46.0f * power, kStart.y - dy / len * 46.0f * power };
                DrawLineEx({ kStart.x + 8.0f, kStart.y - 36.0f }, drawPos, 4.0f, cGold);
                DrawLineEx({ kStart.x + 8.0f, kStart.y + 36.0f }, drawPos, 4.0f, cGold);

                // Vista previa de la trayectoria: solo el principio, el resto hay que calcularlo a ojo
                float previewT = (data.difficulty == 0) ? 1.5f : (data.difficulty == 2 ? 0.6f : 1.0f);
                Vector2 p = kStart, v = { dx / len * vmax * power, dy / len * vmax * power };
                int steps = static_cast<int>(previewT / kStep);
                for (int n = 1; n <= steps; ++n) {
                    GravStep(p, v, kStep);
                    if (n % 10 == 0) {
                        unsigned char a = static_cast<unsigned char>(255.0f * (1.0f - 0.7f * n / steps));
                        DrawRectangleRec({ p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f }, Color{ 255, 255, 255, a });
                    }
                }
            }
        } else if (std::fmod(tNow, 1.0f) < 0.6f) {
            // pista: de dónde tirar
            DrawRectangleRec({ kStart.x - 74.0f, kStart.y + 30.0f, 8.0f, 8.0f }, cGold);
            DrawRectangleRec({ kStart.x - 58.0f, kStart.y + 24.0f, 8.0f, 8.0f }, cGold);
            DrawRectangleRec({ kStart.x - 42.0f, kStart.y + 18.0f, 8.0f, 8.0f }, cGold);
        }
        DrawRectangleRec({ kStart.x + 4.0f, kStart.y - 40.0f, 8.0f, 8.0f }, cGold);
        DrawRectangleRec({ kStart.x + 4.0f, kStart.y + 32.0f, 8.0f, 8.0f }, cGold);
    }

    // El cohete, centrado en su posición y apuntando hacia donde va
    if (!(sling == SlingState::MISS && std::fmod(slingTimer, 0.24f) < 0.12f) &&
        std::hypot(drawPos.x - kEarth.x, drawPos.y - kEarth.y) > 140.0f) {
        const float sc = 0.40f;
        float rad = shipAngle * 0.0174533f;
        Vector2 base = { drawPos.x - std::cos(rad) * 125.0f * sc, drawPos.y + std::sin(rad) * 125.0f * sc };
        bool sputter = (driftTimer >= 0.0f && driftTimer < 0.9f && std::fmod(driftTimer, 0.22f) < 0.1f);
        DrawModularRocket(data, base.x, base.y, sc, shipAngle, tliBurning || sputter, false);
    }

    if (driftTimer >= 0.0f) {
        // Aviso de combustible agotado
        bool blink = std::fmod(driftTimer, 0.7f) < 0.4f;
        Color warn = blink ? Color{ 242, 193, 78, 255 } : Color{ 232, 99, 44, 255 };
        DrawRectangle(246, 491, 788, 203, kInk);
        DrawRectangle(250, 495, 780, 195, warn);
        DrawRectangle(256, 501, 768, 183, Color{ 12, 18, 34, 250 });
        const char* t1 = Tr(data.lang, "¡COMBUSTIBLE AGOTADO!", "OUT OF FUEL!");
        DrawUIText(t1, 640.0f - MeasureUIText(t1, 38.0f) * 0.5f, 516.0f, 38.0f, warn);
        const char* t2 = TextFormat(Tr(data.lang,
            "Al cohete le faltaban %.0f m/s de Delta-V para llegar a la Luna.",
            "The rocket was %.0f m/s of Delta-V short of reaching the Moon."),
            std::max(0.0f, 5800.0f - data.rocket.deltaV));
        DrawUIText(t2, 640.0f - MeasureUIText(t2, 21.0f) * 0.5f, 572.0f, 21.0f, WHITE);
        const char* t3 = Tr(data.lang, "La nave se queda dando vueltas sin control...", "The ship is left tumbling with no control...");
        DrawUIText(t3, 640.0f - MeasureUIText(t3, 19.0f) * 0.5f, 604.0f, 19.0f, cDim);
        DrawRectangle(436, 642, 408, 26, kInk);
        DrawRectangle(440, 646, 400, 18, Color{ 40, 50, 84, 255 });
        DrawRectangle(440, 646, 10, 18, Color{ 217, 83, 79, 255 });
        return;
    }

    // Panel de la etapa, abajo a la izquierda para dejar libre la zona de tiro
    DrawRectangle(16, 592, 668, 116, kInk);
    DrawRectangle(20, 596, 660, 108, Color{ 12, 18, 34, 240 });
    DrawRectangleLinesEx({ 20.0f, 596.0f, 660.0f, 108.0f }, 2.0f, cGold);
    DrawUIText(Tr(data.lang, "ETAPA 2 · INYECCIÓN TRANSLUNAR", "STAGE 2 · TRANSLUNAR INJECTION"), 36.0f, 606.0f, 19.0f, cGold);
    DrawUITextBoxed(Tr(data.lang,
        "Arrastra desde el cohete hacia atrás y suelta. La gravedad curva el tiro: entra en el anillo de la Luna sin chocar con los asteroides.",
        "Drag back from the rocket and release. Gravity bends the shot: enter the ring around the Moon without hitting the asteroids."),
        { 36.0f, 634.0f, 420.0f, 66.0f }, 16.0f, 4.0f, WHITE);

    DrawUIText(Tr(data.lang, "TIROS:", "SHOTS:"), 480.0f, 608.0f, 18.0f, cDim);
    int totalShots = (data.difficulty == 0) ? 4 : 3;
    for (int k = 0; k < totalShots; ++k) {
        DrawRectangleRec({ 549.0f + k * 26.0f, 607.0f, 22.0f, 22.0f }, kInk);
        DrawRectangleRec({ 552.0f + k * 26.0f, 610.0f, 16.0f, 16.0f }, k < shotsLeft ? Color{ 88, 179, 104, 255 } : Color{ 40, 50, 84, 255 });
    }
    DrawUIText(Tr(data.lang, "POTENCIA:", "POWER:"), 480.0f, 642.0f, 18.0f, cDim);
    DrawRectangle(477, 668, 190, 22, kInk);
    DrawRectangle(480, 671, 184, 16, Color{ 40, 50, 84, 255 });
    DrawRectangle(480, 671, static_cast<int>(184.0f * power), 16, power > 0.9f ? Color{ 232, 99, 44, 255 } : cGold);

    // Mensajes
    if (sling == SlingState::MISS) {
        const char* m = Tr(data.lang, missEs.c_str(), missEn.c_str());
        float mw = MeasureUIText(m, 28.0f);
        DrawRectangle(static_cast<int>(640.0f - mw * 0.5f - 24.0f), 76, static_cast<int>(mw + 48.0f), 56, kInk);
        DrawRectangle(static_cast<int>(640.0f - mw * 0.5f - 20.0f), 80, static_cast<int>(mw + 40.0f), 48, Color{ 12, 18, 34, 245 });
        DrawUIText(m, 640.0f - mw * 0.5f, 90.0f, 28.0f, Color{ 242, 160, 70, 255 });
        const char* left = (shotsLeft > 0)
            ? TextFormat(Tr(data.lang, "Te quedan %d tiros", "%d shots left"), shotsLeft)
            : Tr(data.lang, "No quedan tiros", "No shots left");
        DrawUIText(left, 640.0f - MeasureUIText(left, 19.0f) * 0.5f, 140.0f, 19.0f, cDim);
    } else if (sling == SlingState::CAPTURED) {
        const char* m = Tr(data.lang, "¡EN RUMBO A LA LUNA!", "ON COURSE FOR THE MOON!");
        float mw = MeasureTitleText(m, 30.0f);
        DrawTitleText(m, 640.0f - mw * 0.5f, 90.0f, 30.0f, Color{ 88, 179, 104, 255 });
    }
}

void MissionPhase::DrawLandingScene(const SharedMissionData& data) const {
    DrawSky(1.0f);

    // Tierra lejana en el cielo negro lunar
    if (texEarth.id > 0) DrawSpriteCentered(texEarth, 1040.0f, 145.0f, 92.0f);
    else DrawCircle(1040, 145, 42, Color{ 42, 118, 212, 255 });

    // Superficie craterizada del Polo Sur Lunar
    float groundY = 605.0f;
    // Suelo lunar con la misma paleta que la Luna pixel-art del viaje
    DrawRectangle(0, static_cast<int>(groundY), SCREEN_WIDTH, 130, Color{ 182, 203, 208, 255 });
    DrawRectangle(0, static_cast<int>(groundY), SCREEN_WIDTH, 6, Color{ 211, 235, 239, 255 });
    DrawRectangle(0, static_cast<int>(groundY) - 4, SCREEN_WIDTH, 4, Color{ 34, 26, 52, 255 });
    DrawEllipse(220, 640, 95, 18, Color{ 34, 26, 52, 255 });
    DrawEllipse(220, 642, 89, 14, Color{ 144, 161, 168, 255 });
    DrawEllipse(420, 655, 70, 14, Color{ 34, 26, 52, 255 });
    DrawEllipse(420, 657, 64, 10, Color{ 144, 161, 168, 255 });
    DrawEllipse(880, 650, 85, 16, Color{ 34, 26, 52, 255 });
    DrawEllipse(880, 652, 79, 12, Color{ 144, 161, 168, 255 });
    DrawEllipse(1090, 642, 105, 20, Color{ 34, 26, 52, 255 });
    DrawEllipse(1090, 644, 99, 16, Color{ 144, 161, 168, 255 });

    // Zona segura de alunizaje iluminada en el centro (X = 640)
    DrawRectangle(545, static_cast<int>(groundY - 4), 190, 8, GREEN);
    DrawUIText("LZ-ARTEMIS", 595.0f, groundY + 12.0f, 16.0f, Color{ 20, 110, 50, 255 }, false);

    // Modulo Lunar descendiendo
    float landerScreenX = 640.0f + landerPosX;
    float landerScreenY = groundY - (landerAltMeters / 1200.0f) * 460.0f;

    bool retroFiring = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) && (landerFuelPct > 0.0f);

    if (texLander.id > 0) {
        // Módulo lunar del equipo: las patas tocan el suelo en landerScreenY
        const float lw = 104.0f;
        float lh = lw * static_cast<float>(texLander.height) / static_cast<float>(texLander.width);
        if (retroFiring) {
            // llama pixel bajo la tobera, por capas como la del cohete
            float t = static_cast<float>(GetTime());
            float len = 46.0f + 8.0f * std::sin(t * 23.0f) + 5.0f * std::sin(t * 37.0f + 1.0f);
            int rows = static_cast<int>(len / 5.0f);
            for (int i = 0; i < rows; ++i) {
                float u = static_cast<float>(i) / rows;
                float half = std::max(2.0f, (1.0f - u) * 11.0f);
                float fx = landerScreenX + std::sin(t * 18.0f - i * 0.6f) * u * 4.0f;
                float fy = landerScreenY - 12.0f + i * 5.0f;
                DrawRectangleRec({ fx - half - 3.0f, fy, 2.0f * half + 6.0f, 6.0f }, Color{ 158, 42, 28, 255 });
                DrawRectangleRec({ fx - half, fy, 2.0f * half, 6.0f }, Color{ 232, 99, 44, 255 });
                if (u < 0.7f)  DrawRectangleRec({ fx - half * 0.6f, fy, 2.0f * half * 0.6f, 6.0f }, Color{ 242, 193, 78, 255 });
                if (u < 0.35f) DrawRectangleRec({ fx - half * 0.3f, fy, 2.0f * half * 0.3f, 6.0f }, Color{ 255, 243, 176, 255 });
            }
        }
        DrawTexturePro(texLander, { 0, 0, (float)texLander.width, (float)texLander.height },
                       { landerScreenX - lw * 0.5f, landerScreenY - lh, lw, lh }, { 0, 0 }, 0.0f, WHITE);
    } else {
        if (retroFiring) {
            DrawTriangle({ landerScreenX - 12.0f, landerScreenY },
                         { landerScreenX + 12.0f, landerScreenY },
                         { landerScreenX, landerScreenY + 38.0f + GetRandomValue(0, 12) }, GOLD);
        }

        // Patas desplegadas del modulo lunar
        DrawLineEx({ landerScreenX - 18.0f, landerScreenY - 12.0f }, { landerScreenX - 34.0f, landerScreenY }, 3.0f, LIGHTGRAY);
        DrawLineEx({ landerScreenX + 18.0f, landerScreenY - 12.0f }, { landerScreenX + 34.0f, landerScreenY }, 3.0f, LIGHTGRAY);

        // Cuerpo dorado/blanco del modulo lunar
        DrawRectangle(static_cast<int>(landerScreenX - 22), static_cast<int>(landerScreenY - 34), 44, 22, Color{ 218, 172, 46, 255 });
        DrawRectangleLines(static_cast<int>(landerScreenX - 22), static_cast<int>(landerScreenY - 34), 44, 22, BLACK);
        DrawRectangle(static_cast<int>(landerScreenX - 16), static_cast<int>(landerScreenY - 54), 32, 20, Color{ 238, 242, 248, 255 });
        DrawRectangleLines(static_cast<int>(landerScreenX - 16), static_cast<int>(landerScreenY - 54), 32, 20, DARKGRAY);

    }

    // Panel de telemetria de descenso
    DrawRectangle(20, 80, 345, 195, Color{ 12, 18, 34, 235 });
    DrawRectangleLinesEx({ 20.0f, 80.0f, 345.0f, 195.0f }, 2.0f, GOLD);
    DrawUIText(Tr(data.lang, "ETAPA 3 · DESCENSO LUNAR", "STAGE 3 · LUNAR DESCENT"), 36.0f, 92.0f, 18.0f, GOLD);
    DrawUIText(TextFormat(Tr(data.lang, "Altitud: %.0f m", "Altitude: %.0f m"), landerAltMeters), 36.0f, 122.0f, 17.0f, WHITE);
    Color velCol = (std::fabs(landerVelY) <= 6.2f) ? GREEN : Color{ 255, 100, 100, 255 };
    DrawUIText(TextFormat(Tr(data.lang, "Vel. Vertical: %.1f m/s (<= 6,2)", "Vertical Speed: %.1f m/s (<= 6.2)"), landerVelY), 36.0f, 150.0f, 16.0f, velCol);
    DrawUIText(TextFormat(Tr(data.lang, "Combustible Lander: %.0f%%", "Lander Fuel: %.0f%%"), landerFuelPct), 36.0f, 178.0f, 16.0f, SKYBLUE);
    DrawUIText(Tr(data.lang, "W / ESPACIO: Frenar descenso", "W / SPACE: Retro-thrusters"), 36.0f, 212.0f, 15.0f, LIGHTGRAY);
    DrawUIText(Tr(data.lang, "A / D: Alinear con zona verde", "A / D: Align with green pad"), 36.0f, 238.0f, 15.0f, LIGHTGRAY);

    Rectangle btnRetro = { 490.0f, 628.0f, 300.0f, 52.0f };
    DrawRectangleRec(btnRetro, retroFiring ? ORANGE : Color{ 34, 86, 152, 255 });
    DrawRectangleLinesEx(btnRetro, 2.0f, WHITE);
    const char* rTxt = Tr(data.lang, "FRENAR DESCENSO (ESPACIO)", "FIRE RETRO-THRUST (SPACE)");
    float rw = MeasureUIText(rTxt, 18.0f);
    DrawUIText(rTxt, btnRetro.x + (btnRetro.width - rw) * 0.5f, btnRetro.y + 16.0f, 18.0f, WHITE);
}

void MissionPhase::DrawDebriefModal(const SharedMissionData& data) const {
    // Informe final con el mismo estilo de paneles que el menú. Si la misión falla, el tono es
    // de "casi": colores cálidos en vez de rojo y un consejo para el siguiente intento.
    const Color accent = success ? Color{ 88, 179, 104, 255 } : Color{ 242, 160, 70, 255 };
    const Color cGold  = { 242, 193, 78, 255 };
    const Color cDim   = { 201, 208, 240, 255 };
    Vector2 mouse = GetMousePosition();
    float tNow = static_cast<float>(GetTime());

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 6, 10, 20, 205 });

    DrawRectangle(209, 124, 862, 477, kInk);
    DrawRectangle(215, 130, 850, 465, accent);
    DrawRectangle(221, 136, 838, 453, Color{ 13, 20, 38, 255 });
    DrawRectangle(221, 136, 838, 40, Color{ 24, 34, 64, 255 });

    DrawUIText(Tr(data.lang, "INFORME DE MISIÓN · ARTEMIS 4", "MISSION REPORT · ARTEMIS 4"), 250.0f, 146.0f, 20.0f, Color{ 127, 209, 255, 255 });
    const char* mood = success ? Tr(data.lang, "¡MISIÓN CUMPLIDA!", "MISSION ACCOMPLISHED!")
                               : Tr(data.lang, "CASI... ¡A POR EL SIGUIENTE INTENTO!", "SO CLOSE... ON TO THE NEXT TRY!");
    DrawUIText(mood, 940.0f - MeasureUIText(mood, 18.0f), 148.0f, 18.0f, accent);

    {
        // el título se encoge si es largo, para no pisar la insignia de la nota
        const char* title = Tr(data.lang, resultTitleEs.c_str(), resultTitleEn.c_str());
        float ts = 28.0f;
        while (ts > 16.0f && MeasureUIText(title, ts) > 680.0f) ts -= 1.0f;
        DrawUIText(title, 250.0f, 192.0f + (28.0f - ts) * 0.5f, ts, accent);
    }

    // Insignia con la nota
    float bob = success ? std::sin(tNow * 4.0f) * 3.0f : 0.0f;
    DrawRectangle(951, static_cast<int>(146 + bob), 92, 84, kInk);
    DrawRectangle(955, static_cast<int>(150 + bob), 84, 76, success ? Color{ 44, 138, 72, 255 } : Color{ 60, 70, 110, 255 });
    DrawRectangle(955, static_cast<int>(150 + bob), 84, 6, Color{ 255, 255, 255, 40 });
    {
        float gw = MeasureTitleText(grade.c_str(), 40.0f);
        DrawTitleText(grade.c_str(), 997.0f - gw * 0.5f, 170.0f + bob, 40.0f, cGold);
    }

    DrawUITextBoxed(Tr(data.lang, resultDetailEs.c_str(), resultDetailEn.c_str()),
                    { 250.0f, 240.0f, 680.0f, 100.0f }, 20.0f, 5.0f, WHITE);

    DrawRectangle(246, 341, 788, 153, kInk);
    DrawRectangle(250, 345, 780, 145, Color{ 20, 30, 54, 255 });

    DrawUIText(TextFormat(Tr(data.lang,
        "Cohete: %s + %s + %s",
        "Vehicle: %s + %s + %s"),
        data.rocket.capsule.name.c_str(), data.rocket.tank.name.c_str(), data.rocket.engine.name.c_str()),
        270.0f, 356.0f, 18.0f, cGold);

    DrawUIText(TextFormat(Tr(data.lang,
        "Altitud máx: %.1f km   |   Vel. máx: %.2f km/s   |   Vel. alunizaje: %.1f m/s",
        "Peak altitude: %.1f km   |   Peak velocity: %.2f km/s   |   Touchdown: %.1f m/s"),
        altitudeKm, velocityKms, touchdownSpeed),
        270.0f, 388.0f, 18.0f, WHITE);

    DrawUIText(TextFormat(Tr(data.lang,
        "Coste del cohete: $%.0fM   |   TWR: %.2f   |   Delta-V: %.0f m/s",
        "Rocket cost: $%.0fM   |   TWR: %.2f   |   Delta-V: %.0f m/s"),
        data.rocket.totalCost, data.rocket.twr, data.rocket.deltaV),
        270.0f, 420.0f, 18.0f, cDim);

    DrawUIText(success
        ? Tr(data.lang, "¿Te atreves a repetirlo con menos presupuesto o más dificultad?", "Dare to do it again with a smaller budget or a harder setting?")
        : Tr(data.lang, "Los fallos también son datos: cambia algo en el hangar y vuelve a probar.", "Failures are data too: change something in the hangar and try again."),
        270.0f, 454.0f, 18.0f, accent);

    auto button = [&](Rectangle r, Color base, const char* label) {
        bool hover = CheckCollisionPointRec(mouse, r);
        auto shade = [](Color c, int d) {
            return Color{ static_cast<unsigned char>(std::clamp(c.r + d, 0, 255)), static_cast<unsigned char>(std::clamp(c.g + d, 0, 255)),
                          static_cast<unsigned char>(std::clamp(c.b + d, 0, 255)), 255 };
        };
        Color b = hover ? shade(base, 26) : base;
        DrawRectangleRec({ r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f }, kInk);
        DrawRectangleRec(r, b);
        DrawRectangleRec({ r.x, r.y, r.width, 4.0f }, shade(b, 46));
        DrawRectangleRec({ r.x, r.y + r.height - 5.0f, r.width, 5.0f }, shade(b, -42));
        DrawUIText(label, r.x + (r.width - MeasureUIText(label, 21.0f)) * 0.5f, r.y + 13.0f, 21.0f, WHITE);
    };
    button({ 245.0f, 520.0f, 240.0f, 48.0f }, Color{ 36, 92, 170, 255 }, Tr(data.lang, "REPETIR VUELO", "RETRY FLIGHT"));
    button({ 515.0f, 520.0f, 250.0f, 48.0f }, Color{ 190, 130, 30, 255 }, Tr(data.lang, "IR AL HANGAR", "GO TO HANGAR"));
    button({ 795.0f, 520.0f, 240.0f, 48.0f }, Color{ 44, 138, 72, 255 }, Tr(data.lang, "MENÚ PRINCIPAL", "MAIN MENU"));
}

void MissionPhase::DrawEndingScene(const SharedMissionData& data) const {
    float t = endingTimer, tNow = static_cast<float>(GetTime());
    const Color cGold = { 242, 193, 78, 255 };
    const Color cDim  = { 201, 208, 240, 255 };
    const float groundY = 540.0f;

    // Cielo, la Tierra saliendo sobre el horizonte y el cohete pasando en órbita
    DrawSky(1.0f);
    if (texEarth.id > 0) DrawSpriteCentered(texEarth, 230.0f, 230.0f - std::min(60.0f, t * 6.0f), 210.0f);
    {
        Texture2D rk = texFinalRocket[RocketModelFromParts(data) - 1];
        if (rk.id > 0 && t > 6.5f) {
            float cyc = std::fmod(t - 6.5f, 11.0f) / 11.0f;
            float rx = -160.0f + cyc * (SCREEN_WIDTH + 320.0f), ry = 248.0f + std::sin(tNow * 1.5f) * 4.0f;
            float len = 104.0f, thick = len * static_cast<float>(rk.width) / static_cast<float>(rk.height);
            for (int i = 0; i < 8; ++i) {
                float u = i / 8.0f, half = (1.0f - u) * 9.0f + 2.0f, fx = rx - len * 0.5f - 2.0f - i * 6.0f;
                DrawRectangleRec({ fx - 6.0f, ry - half - 2.0f, 7.0f, 2.0f * half + 4.0f }, Color{ 158, 42, 28, 255 });
                DrawRectangleRec({ fx - 6.0f, ry - half, 7.0f, 2.0f * half }, Color{ 232, 99, 44, 255 });
                if (u < 0.6f) DrawRectangleRec({ fx - 6.0f, ry - half * 0.5f, 7.0f, half }, cGold);
            }
            DrawTexturePro(rk, { 0, 0, (float)rk.width, (float)rk.height }, { rx, ry, thick, len }, { thick * 0.5f, len * 0.5f }, 90.0f, WHITE);
        }
    }

    // Fuegos artificiales en bloques
    if (t > kEndParty) {
        const Color fw[5] = { { 242, 193, 78, 255 }, { 127, 209, 255, 255 }, { 88, 179, 104, 255 }, { 240, 120, 170, 255 }, { 255, 255, 255, 255 } };
        for (int b = 0; b < 7; ++b) {
            float age = std::fmod(t - kEndParty - b * 0.7f, 4.9f);
            if (age < 0.0f || age > 1.3f) continue;
            int round = static_cast<int>((t - kEndParty - b * 0.7f) / 4.9f);
            float cx = 150.0f + Hash(b * 7 + round * 13 + 1) * 980.0f, cy = 90.0f + Hash(b * 11 + round * 5 + 3) * 230.0f;
            float r = (1.0f - std::pow(1.0f - age / 1.3f, 2.0f)) * 110.0f;
            unsigned char a = static_cast<unsigned char>(255.0f * (1.0f - age / 1.3f));
            Color c = fw[(b + round) % 5]; c.a = a;
            for (int k = 0; k < 16; ++k) {
                float ang = k * 0.3927f;
                PxRect(cx + std::cos(ang) * r, cy + std::sin(ang) * r + age * age * 26.0f, 8.0f, 8.0f, c);
                PxRect(cx + std::cos(ang + 0.2f) * r * 0.55f, cy + std::sin(ang + 0.2f) * r * 0.55f + age * age * 26.0f, 4.0f, 4.0f, c);
            }
        }
    }

    // Suelo lunar con la paleta de la Luna pixel-art
    DrawRectangle(0, static_cast<int>(groundY), SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 182, 203, 208, 255 });
    DrawRectangle(0, static_cast<int>(groundY), SCREEN_WIDTH, 8, Color{ 211, 235, 239, 255 });
    DrawRectangle(0, static_cast<int>(groundY) - 4, SCREEN_WIDTH, 4, kInk);
    const float craters[5][3] = { { 150, 628, 90 }, { 640, 676, 120 }, { 1010, 690, 80 }, { 1150, 602, 70 }, { 330, 700, 60 } };
    for (const auto& c : craters) {
        PxRect(c[0] - c[2], c[1] - c[2] * 0.2f, c[2] * 2.0f, c[2] * 0.4f, kInk);
        PxRect(c[0] - c[2] + 4.0f, c[1] - c[2] * 0.2f + 4.0f, c[2] * 2.0f - 8.0f, c[2] * 0.4f - 4.0f, Color{ 144, 161, 168, 255 });
        PxRect(c[0] - c[2] + 4.0f, c[1] - c[2] * 0.2f + 4.0f, c[2] * 2.0f - 8.0f, 8.0f, Color{ 104, 113, 122, 255 });
    }
    // La gamba también ha venido: asoma por un cráter al final
    if (texShrimp.id > 0 && t > kEndParty + 3.5f) {
        float up = std::min(1.0f, (t - kEndParty - 3.5f) / 0.6f);
        float sw = 54.0f, sh = sw * static_cast<float>(texShrimp.height) / static_cast<float>(texShrimp.width);
        DrawTexturePro(texShrimp, { 0, 0, (float)texShrimp.width, (float)texShrimp.height * (0.25f + 0.75f * up) },
                       { 1124.0f, 592.0f - sh * (0.25f + 0.75f * up) + 8.0f, sw, sh * (0.25f + 0.75f * up) }, { 0, 0 }, 0.0f, WHITE);
    }

    // Módulo lunar posado
    const float landerX = 930.0f, landerFeet = 606.0f;
    if (texLander.id > 0) {
        float lw = 250.0f, lh = lw * static_cast<float>(texLander.height) / static_cast<float>(texLander.width);
        DrawTexturePro(texLander, { 0, 0, (float)texLander.width, (float)texLander.height },
                       { landerX - lw * 0.5f, landerFeet - lh, lw, lh }, { 0, 0 }, 0.0f, WHITE);
    }

    // El astronauta: baja, camina a saltos de baja gravedad, clava la bandera y lo celebra
    const float flagX = 356.0f, footY = 612.0f, startX = 800.0f, endX = 520.0f;
    float ax = startX, hop = 0.0f;
    bool visible = (t > kEndWalk0 - 0.4f);
    if (t < kEndWalk0) {
        ax = startX;
    } else if (t < kEndWalk1) {
        float k = (t - kEndWalk0) / (kEndWalk1 - kEndWalk0);
        ax = startX + (endX - startX) * k;
        hop = std::fabs(std::sin((t - kEndWalk0) / kEndHop * 3.14159f)) * 58.0f;
    } else {
        ax = endX;
        if (t > kEndParty) hop = std::fabs(std::sin((t - kEndParty) / 1.1f * 3.14159f)) * 92.0f;
    }
    // huellas que se quedan (en la Luna duran millones de años)
    if (t > kEndWalk0) {
        float walked = std::min(t, kEndWalk1) - kEndWalk0;
        int steps = static_cast<int>(walked / kEndHop);
        for (int k = 0; k <= steps; ++k) {
            float fx = startX + (endX - startX) * (k * kEndHop / (kEndWalk1 - kEndWalk0));
            PxRect(fx - 18.0f, footY - 2.0f, 12.0f, 4.0f, Color{ 104, 113, 122, 255 });
            PxRect(fx + 6.0f, footY + 2.0f, 12.0f, 4.0f, Color{ 104, 113, 122, 255 });
        }
    }
    // bandera
    if (t > kEndWalk1) {
        float rise = std::min(1.0f, (t - kEndWalk1) / (kEndParty - kEndWalk1));
        float poleH = 190.0f * rise;
        PxRect(flagX - 4.0f, footY - poleH - 4.0f, 12.0f, poleH + 8.0f, kInk);
        PxRect(flagX, footY - poleH, 4.0f, poleH, Color{ 211, 235, 239, 255 });
        if (rise >= 1.0f) {
            float fw = 96.0f, fh = 60.0f, fy = footY - poleH;
            PxRect(flagX + 4.0f, fy - 4.0f, fw + 8.0f, fh + 8.0f, kInk);
            for (int col = 0; col < 12; ++col) {   // ondea un poco, aunque en la Luna no haya viento
                float wy = std::sin(tNow * 4.0f + col * 0.6f) * 3.0f * (col / 12.0f);
                DrawRectangleRec({ flagX + 8.0f + col * 8.0f, fy + wy, 8.0f, fh }, Color{ 32, 68, 128, 255 });
                DrawRectangleRec({ flagX + 8.0f + col * 8.0f, fy + wy, 8.0f, 6.0f }, Color{ 127, 209, 255, 255 });
            }
            DrawUIText("A4", flagX + 30.0f, fy + 12.0f, 34.0f, cGold);
        }
    }
    if (visible && texAstronaut.id > 0) {
        float ah = 196.0f, aw = ah * static_cast<float>(texAstronaut.width) / static_cast<float>(texAstronaut.height);
        // sombra y polvo al caer
        float sh = 1.0f - hop / 140.0f;
        DrawRectangleRec({ ax - 44.0f * sh, footY - 2.0f, 88.0f * sh, 8.0f }, Color{ 104, 113, 122, 160 });
        if (hop < 10.0f && t > kEndWalk0) {
            for (int k = 0; k < 4; ++k) PxRect(ax - 60.0f + k * 36.0f + std::sin(tNow * 9.0f + k) * 4.0f, footY - 10.0f - (k % 2) * 8.0f, 8.0f, 8.0f, Color{ 211, 235, 239, 200 });
        }
        unsigned char al = static_cast<unsigned char>(255.0f * std::clamp((t - (kEndWalk0 - 0.4f)) / 0.4f, 0.0f, 1.0f));
        DrawTexturePro(texAstronaut, { 0, 0, (float)texAstronaut.width, (float)texAstronaut.height },
                       { ax - aw * 0.5f, footY - ah - hop, aw, ah }, { 0, 0 }, 0.0f, Color{ 255, 255, 255, al });

        // bocadillo de celebración
        if (t > kEndParty + 0.2f) {
            const char* shoutsEs[3] = { "¡WUHUUU!", "¡VIVA!", "¡BIEEEN!" };
            const char* shoutsEn[3] = { "WOOHOO!", "HOORAY!", "YESSS!" };
            int which = static_cast<int>((t - kEndParty) / 1.1f) % 3;
            const char* shout = Tr(data.lang, shoutsEs[which], shoutsEn[which]);
            float bw = MeasureUIText(shout, 30.0f) + 36.0f;
            float bx = ax - bw * 0.5f, by = footY - ah - hop - 70.0f;
            DrawRectangleRec({ bx - 4.0f, by - 4.0f, bw + 8.0f, 56.0f }, kInk);
            DrawRectangleRec({ bx, by, bw, 48.0f }, WHITE);
            DrawRectangleRec({ ax - 8.0f, by + 48.0f, 16.0f, 10.0f }, WHITE);
            DrawUIText(shout, bx + 18.0f, by + 8.0f, 30.0f, Color{ 34, 26, 52, 255 }, false);
        }
    }

    // Títulos
    if (t > kEndParty) {
        float pop = std::min(1.0f, (t - kEndParty) / 0.35f);
        float ts = 44.0f * (0.6f + 0.4f * pop) * (1.0f + 0.03f * std::sin(tNow * 5.0f));
        const char* title = Tr(data.lang, "¡LLEGAMOS A LA LUNA!", "WE MADE IT TO THE MOON!");
        float tw = MeasureTitleText(title, ts);
        DrawTitleText(title, 640.0f - tw * 0.5f, 62.0f, ts, cGold);
        const char* sub = Tr(data.lang, "MISIÓN ARTEMIS 4 CUMPLIDA", "ARTEMIS 4 MISSION ACCOMPLISHED");
        DrawUIText(sub, 640.0f - MeasureUIText(sub, 28.0f) * 0.5f, 128.0f, 28.0f, Color{ 127, 209, 255, 255 });
    }
    if (t > kEndParty + 3.0f) {
        unsigned char a = static_cast<unsigned char>(255.0f * std::min(1.0f, (t - kEndParty - 3.0f) / 0.8f));
        const char* by = Tr(data.lang, "UN JUEGO DE", "A GAME BY");
        DrawRectangle(0, 646, SCREEN_WIDTH, 74, Color{ 13, 20, 38, static_cast<unsigned char>(a * 0.86f) });
        DrawRectangle(0, 646, SCREEN_WIDTH, 4, Color{ 242, 193, 78, a });
        DrawUIText(by, 24.0f, 658.0f, 16.0f, Color{ 242, 193, 78, a });
        DrawUIText("Claudia Sanz · Rafael Lizasoain · Gonzalo de Sagarra · Sol Gonzalez · Martina Ruipérez", 24.0f, 682.0f, 20.0f, Color{ 255, 255, 255, a });
    }
    if (t > 2.5f && std::fmod(tNow, 1.0f) < 0.6f) {
        const char* hint = Tr(data.lang, "CLIC para ver el informe >", "CLICK to see the report >");
        DrawUIText(hint, SCREEN_WIDTH - MeasureUIText(hint, 17.0f) - 24.0f, 20.0f, 17.0f, cDim);
    }

    // fundido de entrada
    if (t < 1.0f) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 0, 0, 0, static_cast<unsigned char>(255.0f * (1.0f - t)) });
}

void MissionPhase::Draw(const SharedMissionData& data) {
    ClearBackground(Color{ 8, 12, 24, 255 });

    if (stage == FlightStage::DEBRIEF && endingTimer >= 0.0f) {
        DrawEndingScene(data);
        return;
    }

    if (stage == FlightStage::COUNTDOWN_ASCENT) {
        DrawAscentScene(data);
    } else if (stage == FlightStage::TLI_BURN) {
        DrawTLIScene(data);
    } else {
        DrawLandingScene(data);
    }

    // Barra superior fija
    DrawRectangle(0, 0, SCREEN_WIDTH, 60, Color{ 10, 15, 28, 245 });
    DrawRectangle(20, 14, 100, 34, Color{ 25, 35, 60, 255 });
    DrawRectangleLines(20, 14, 100, 34, GOLD);
    DrawUIText("MENÚ", 44.0f, 21.0f, 18.0f, WHITE);

    DrawRectangle(130, 14, 175, 34, Color{ 35, 55, 95, 255 });
    DrawRectangleLines(130, 14, 175, 34, SKYBLUE);
    DrawUIText(Tr(data.lang, "< AL HANGAR", "< TO HANGAR"), 148.0f, 21.0f, 17.0f, WHITE);

    if (stage == FlightStage::COUNTDOWN_ASCENT) {
        DrawRectangle(315, 14, 125, 34, Color{ 45, 85, 55, 255 });
        DrawRectangleLines(315, 14, 125, 34, GREEN);
        DrawUIText(TextFormat("WARP: %dx", timeWarp), 334.0f, 21.0f, 17.0f, WHITE);

        DrawRectangle(450, 14, 175, 34, autoPilot ? Color{ 38, 125, 62, 255 } : Color{ 45, 52, 70, 255 });
        DrawRectangleLines(450, 14, 175, 34, autoPilot ? GREEN : GRAY);
        DrawUIText(autoPilot ? "AUTO-PILOT: ON" : "AUTO-PILOT: OFF", 464.0f, 21.0f, 16.0f, WHITE);
    }

    DrawUIText(TextFormat(Tr(data.lang,
        "ALT: %.1f km   |   VEL: %.2f km/s   |   FUEL: %.1f t",
        "ALT: %.1f km   |   VEL: %.2f km/s   |   FUEL: %.1f t"),
        altitudeKm, velocityKms, fuelRemainingTons),
        665.0f, 19.0f, 21.0f, GOLD);

    if (stage == FlightStage::DEBRIEF) {
        DrawDebriefModal(data);
    }
}

void MissionPhase::Unload() {
    UnloadIncidents();
    if (!spritesLoaded) return;

    for (int i = 0; i < 6; ++i) {
        if (texCapsules[i].id > 0) UnloadTexture(texCapsules[i]);
        if (texLanders[i].id > 0)  UnloadTexture(texLanders[i]);
        if (texTanks[i].id > 0)    UnloadTexture(texTanks[i]);
        if (texEngines[i].id > 0)  UnloadTexture(texEngines[i]);
    }
    for (int i = 0; i < 5; ++i) {
        if (texBoosters[i].id > 0) UnloadTexture(texBoosters[i]);
    }

    if (texLaunchBg.id > 0)    UnloadTexture(texLaunchBg);
    if (texLaunchTower.id > 0) UnloadTexture(texLaunchTower);
    if (texFlameMain1.id > 0)  UnloadTexture(texFlameMain1);
    if (texFlameMain2.id > 0)  UnloadTexture(texFlameMain2);
    if (texFlameSrb1.id > 0)   UnloadTexture(texFlameSrb1);
    if (texFlameSrb2.id > 0)   UnloadTexture(texFlameSrb2);
    if (texSmoke.id > 0)       UnloadTexture(texSmoke);
    if (texSky.id > 0)         UnloadTexture(texSky);
    if (texMoon.id > 0)        UnloadTexture(texMoon);
    if (texEarth.id > 0)       UnloadTexture(texEarth);
    texEarth = Texture2D{};
    if (texLander.id > 0)      UnloadTexture(texLander);
    texLander = Texture2D{};
    if (texAstronaut.id > 0)   UnloadTexture(texAstronaut);
    texAstronaut = Texture2D{};
    if (texShrimp.id > 0)      UnloadTexture(texShrimp);
    texShrimp = Texture2D{};
    for (int i = 0; i < 3; ++i) {
        if (texFinalRocket[i].id > 0) UnloadTexture(texFinalRocket[i]);
        texFinalRocket[i] = Texture2D{};
    }
    texSky = Texture2D{};
    texMoon = Texture2D{};

    spritesLoaded = false;
}