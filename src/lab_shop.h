#pragma once
#include "games_types.h"
#include <string>
#include <vector>

class LabShopPhase {
public:
    void Init();
    void OnEnter(SharedMissionData& data);
    void RestoreFromSave(SharedMissionData& data);
    GameState Update(SharedMissionData& data);
    void Draw(const SharedMissionData& data);
    void Unload();

private:
    enum class PartCategory {
        CAPSULE = 0,
        LANDER  = 1,
        TANK    = 2,
        ENGINE  = 3,
        BOOSTER = 4
    };

    struct ShopItem {
        int id = 0;
        PartCategory category = PartCategory::CAPSULE;
        std::string nameEs;
        std::string nameEn;
        std::string descEs;
        std::string descEn;
        float costMillions = 0.0f;
        float dryMassTons = 0.0f;
        float fuelTons = 0.0f;
        float thrustKN = 0.0f;
        float isp = 0.0f;
        bool owned = false;
    };

    Texture2D texLaboratory{};
    Texture2D texAngar{};
    Texture2D texCapsules[6]{};
    Texture2D texLanders[6]{};
    Texture2D texTanks[6]{};
    Texture2D texEngines[6]{};
    Texture2D texBoosters[5]{};
    Texture2D texScientist{};
    Texture2D texFinalRocket[3]{};   // cohete terminado 1 (mejor) .. 3 (peor)
    bool texturesLoaded = false;

    // Intro en el laboratorio: la científica habla antes de pasar al hangar
    bool introActive = false;
    int introLine = 0;
    float introChars = 0.0f;
    void DrawIntro(const SharedMissionData& data) const;

    // Animación al pulsar "ir a la Luna": humo sobre las piezas y aparece el cohete terminado
    float buildTimer = -1.0f;

    std::vector<ShopItem> catalog;
    PartCategory activeCategory = PartCategory::CAPSULE;
    bool backpackTab = false; // pestaña MOCHILA (provisiones para los problemas del viaje)

    float saveNoticeTimer = 0.0f;
    float alertTimer = 0.0f;
    std::string alertMsgEs;
    std::string alertMsgEn;

    void BuildCatalog();
    void ResetOwnershipToStarter();
    void RecalculateRocket(SharedMissionData& data);
    int GetEquippedIndexForCategory(const SharedMissionData& data, PartCategory cat) const;
    void SetEquippedIndexForCategory(SharedMissionData& data, PartCategory cat, int itemIdx);
    void DrawRocketPreview(const SharedMissionData& data, float centerX, float bottomY, float scale) const;
};