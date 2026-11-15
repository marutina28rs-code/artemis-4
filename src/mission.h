#pragma once
#include "games_types.h"
#include <string>
#include <vector>

class MissionPhase {
public:
    void Init();
    void OnEnter(const SharedMissionData& data);
    GameState Update(SharedMissionData& data);
    void Draw(const SharedMissionData& data);
    void Unload();

private:
    enum class FlightStage {
        COUNTDOWN_ASCENT = 0, // Cuenta atras rapida + despegue desde rampa hasta orbita (180 km)
        TLI_BURN         = 1, // Maniobra de Inyeccion Translunar hacia la Luna
        LUNAR_DESCENT    = 2, // Descenso propulsado y alunizaje en el Polo Sur
        DEBRIEF          = 3  // Informe final de mision
    };

    struct SeparatedBooster {
        bool active = false;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float velX = 0.0f;
        float velY = 0.0f;
        float rotDeg = 0.0f;
    };

    struct StarPoint {
        float x = 0.0f;
        float y = 0.0f;
        float size = 1.5f;
        unsigned char alpha = 200;
    };

    bool spritesLoaded = false;

    // Sprites modulares opcionales (assets/sprites/parts/)
    Texture2D texCapsules[6]{};
    Texture2D texLanders[6]{};
    Texture2D texTanks[6]{};
    Texture2D texEngines[6]{};
    Texture2D texBoosters[5]{};

    // Sprites opcionales de escenario y fuego (assets/sprites/flight/)
    Texture2D texLaunchBg{};
    Texture2D texLaunchTower{};
    Texture2D texFlameMain1{};
    Texture2D texFlameMain2{};
    Texture2D texFlameSrb1{};
    Texture2D texFlameSrb2{};
    Texture2D texSmoke{};

    // Arte común con el resto del juego: cielo, luna y cohete terminado (1 mejor .. 3 peor)
    Texture2D texSky{};
    Texture2D texMoon{};
    Texture2D texFinalRocket[3]{};
    Texture2D texEarth{};
    Texture2D texLander{};      // módulo lunar que aterriza al final
    Texture2D texAstronaut{};
    Texture2D texShrimp{};
    float endingTimer = -1.0f;  // >= 0: escena final tras un alunizaje con éxito (antes del informe)
    void DrawEndingScene(const SharedMissionData& data) const;
    float rumbleTimer = 0.0f;   // sonido de motores
    float driftTimer = -1.0f;   // >= 0: sin combustible en la inyección, la nave va a la deriva

    // Inyección translunar estilo tirachinas: se arrastra desde el cohete, se suelta y la gravedad
    // de la Tierra y de la Luna curva el tiro. Hay que entrar en el anillo de la Luna.
    enum class SlingState { AIM, FLY, MISS, CAPTURED };
    SlingState sling = SlingState::AIM;
    Vector2 shipPos{ 0.0f, 0.0f };
    Vector2 shipVel{ 0.0f, 0.0f };
    Vector2 dragPos{ 0.0f, 0.0f };
    bool dragging = false;
    bool lowFuelShot = false;
    int shotsLeft = 3;
    float flyTime = 0.0f;
    float slingTimer = 0.0f;    // pausa tras fallar un tiro o al entrar en el anillo
    float simAccum = 0.0f;
    float tliClock = 0.0f;
    float shipAngle = 18.0f;
    std::string missEs;
    std::string missEn;
    std::vector<Vector2> trail;
    int lastCountSec = -1;      // pitidos de la cuenta atrás
    void DrawSky(float alpha) const;

    std::vector<StarPoint> stars;

    // Estado de simulacion
    FlightStage stage = FlightStage::COUNTDOWN_ASCENT;
    float countdownTimer = 3.0f;
    float missionClock = 0.0f;
    int timeWarp = 1;
    bool autoPilot = false;

    // Telemetria de ascenso y TLI
    float altitudeKm = 0.0f;
    float velocityKms = 0.0f;
    float fuelRemainingTons = 0.0f;
    float throttle = 1.0f;
    float pitchDeg = 90.0f;
    float padFailTimer = 0.0f;

    bool boostersAttached = false;
    SeparatedBooster leftBooster{};
    SeparatedBooster rightBooster{};

    // Telemetria TLI
    float tliCursor = 0.0f;
    float tliProgress = 0.0f;
    bool tliBurning = false;

    // Telemetria de descenso lunar
    float landerAltMeters = 1200.0f;
    float landerVelY = -28.0f; // m/s (negativo = bajando)
    float landerPosX = 0.0f;   // desplazamiento respecto al centro de la zona de aterrizaje
    float landerVelX = 0.0f;
    float landerFuelPct = 100.0f;
    float touchdownSpeed = 0.0f;

    // Resultado
    bool success = false;
    std::string grade = "B";
    std::string resultTitleEs;
    std::string resultTitleEn;
    std::string resultDetailEs;
    std::string resultDetailEn;

    void LoadSprites();
    void FinishMission(SharedMissionData& data, bool ok, const std::string& missionGrade,
                       const std::string& titleEs, const std::string& titleEn,
                       const std::string& detailEs, const std::string& detailEn);

    void DrawModularRocket(const SharedMissionData& data, float centerX, float bottomY,
                           float scale, float angleDeg, bool firingMain, bool firingBoosters) const;
    void DrawAscentScene(const SharedMissionData& data) const;
    void DrawTLIScene(const SharedMissionData& data) const;
    void DrawLandingScene(const SharedMissionData& data) const;
    void DrawDebriefModal(const SharedMissionData& data) const;
};