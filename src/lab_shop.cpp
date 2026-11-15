#include "lab_shop.h"
#include "save_manager.h"
#include "backpack_items.h"
#include "sfx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <vector>

namespace {
    // Solo se ofrecen 3 piezas por categoría: las que tienen sprite dibujado por el equipo
    // (barata / estándar / premium). El módulo lunar no tiene sprites y queda fijo en el de serie.
    // Índices del catálogo, indexados por PartCategory (CAPSULE, LANDER, TANK, ENGINE, BOOSTER).
    const int kAvail[5][3] = {
        { 0, 2, 3 },     // Mercury-Lite, Mando Apolo, Orion
        { 6, 6, 6 },     // (sin pestaña)
        { 12, 14, 15 },  // Redstone, S-IVB, Central SLS
        { 18, 20, 23 },  // RL-10, 3x RS-25, F-1B
        { 25, 27, 28 }   // Castor-30, SRB 4 seg., SRB 5 seg.
    };
    const float kPartPriceScale = 1.8f;

    // Animación de montaje: el humo cubre las piezas, se cambia al cohete terminado y se despeja
    const float kBuildSwap = 1.7f;
    const float kBuildEnd  = 3.6f;

    // Pestañas visibles: 4 categorías de piezas + MOCHILA
    const int kTabCount = 5;
    const int kTabCategory[4] = { 0, 2, 3, 4 };

    int SnapToAvailable(int cat, int idx) {
        int best = kAvail[cat][0];
        for (int i = 0; i < 3; ++i) {
            if (std::abs(kAvail[cat][i] - idx) < std::abs(best - idx)) best = kAvail[cat][i];
        }
        return best;
    }

    // Anchos (px a escala 1) de cada sprite en el cohete montado; la altura sale de la proporción de la imagen
    const float kEngW = 58.0f, kTankW = 66.0f, kCapW = 74.0f, kBoostW = 64.0f, kBoostOffsetX = 59.0f;

    std::string ToLowerStr(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    Texture2D LoadExactProjectImage(const std::string& exactFileName) {
        namespace fs = std::filesystem;
        std::string appDir = GetApplicationDirectory();

        std::vector<std::string> directCandidates = {
            exactFileName,
            "assets/" + exactFileName,
            "assets/sprites/" + exactFileName,
            "assets/sprites/parts/" + exactFileName,
            "sprites/" + exactFileName,
            "img/" + exactFileName,
            "images/" + exactFileName,
            appDir + exactFileName,
            appDir + "assets/" + exactFileName,
            appDir + "assets/sprites/" + exactFileName,
            appDir + "assets/sprites/parts/" + exactFileName
        };

        for (const auto& path : directCandidates) {
            if (FileExists(path.c_str())) {
                Texture2D tex = LoadTexture(path.c_str());
                if (tex.id > 0) return tex;
            }
        }

        // Búsqueda recursiva en todo el proyecto por nombre exacto (ignorando mayúsculas/minúsculas de extensión)
        std::string targetStem = ToLowerStr(fs::path(exactFileName).stem().string());
        std::vector<fs::path> searchRoots = { fs::current_path() };
        if (!appDir.empty()) {
            searchRoots.emplace_back(appDir);
        }

        for (const auto& root : searchRoots) {
            try {
                if (!fs::exists(root)) continue;
                for (const auto& entry : fs::recursive_directory_iterator(
                         root, fs::directory_options::skip_permission_denied)) {
                    if (!entry.is_regular_file()) continue;

                    std::string stem = ToLowerStr(entry.path().stem().string());
                    std::string ext  = ToLowerStr(entry.path().extension().string());
                    if ((stem == targetStem || stem == targetStem + ".png") &&
                        (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")) {
                        std::string foundPath = entry.path().generic_string();
                        Texture2D tex = LoadTexture(foundPath.c_str());
                        if (tex.id > 0) return tex;
                    }
                }
            } catch (...) {
            }
        }

        return Texture2D{};
    }

    // Frases de la científica en el laboratorio, antes de entrar al hangar
    const int kIntroLines = 3;
    std::string IntroLine(const SharedMissionData& data, int i) {
        if (i == 0) return TextFormat(Tr(data.lang,
            "¡Lo has conseguido! El comité nos ha dado $%.0fM para la misión.",
            "You did it! The committee gave us $%.0fM for the mission."), data.awardedBudget);
        if (i == 1) return Tr(data.lang,
            "¡Vamos a montar el cohete! En el hangar eliges la cápsula, el tanque, el motor y los boosters, y llenas la mochila para el viaje.",
            "Let's build the rocket! In the hangar you choose the capsule, tank, engine and boosters, and pack the backpack for the trip.");
        return Tr(data.lang,
            "Cuanto mejores sean las piezas, mejor será el cohete que despegue. ¡Al hangar!",
            "The better the parts, the better the rocket that lifts off. To the hangar!");
    }

    // Corta una cadena UTF-8 en un número de bytes sin partir un carácter acentuado
    std::string Utf8Prefix(const std::string& s, size_t n) {
        if (n >= s.size()) return s;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        return s.substr(0, n);
    }

    // Dibuja una imagen cubriendo todo el rectángulo (recorta lo que sobra, sin deformar)
    void DrawCover(Texture2D t, Rectangle src, Rectangle dst) {
        float sAsp = src.width / src.height, dAsp = dst.width / dst.height;
        if (sAsp > dAsp) { float w = src.height * dAsp; src.x += (src.width - w) * 0.5f; src.width = w; }
        else             { float h = src.width / dAsp;  src.y += (src.height - h); src.height = h; }
        DrawTexturePro(t, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
    }

    void DrawSpriteFit(Texture2D t, float centerX, float bottomY, float h, bool mirror = false) {
        if (t.id <= 0) return;
        float w = h * static_cast<float>(t.width) / static_cast<float>(t.height);
        DrawTexturePro(t, { 0, 0, (mirror ? -1.0f : 1.0f) * (float)t.width, (float)t.height },
                       { centerX - w * 0.5f, bottomY - h, w, h }, { 0, 0 }, 0.0f, WHITE);
    }
}

void LabShopPhase::BuildCatalog() {
    catalog = {
        // ==================== 0..5: CÁPSULAS ====================
        { 0, PartCategory::CAPSULE,
          "Cápsula Mercury-Lite", "Mercury-Lite Capsule",
          "Cabina suborbital ligera de titanio con torre LES básica.", "Lightweight titanium cabin with basic LES escape tower.",
          60.0f, 4.5f, 0.0f, 0.0f, 0.0f, true },
        { 1, PartCategory::CAPSULE,
          "Cápsula Gemini-X", "Gemini-X Capsule",
          "Cabina biplaza con control orbital y escudo térmico reforzado.", "Two-seat cabin with orbital maneuvering & reinforced heat shield.",
          115.0f, 7.5f, 0.0f, 0.0f, 0.0f, false },
        { 2, PartCategory::CAPSULE,
          "Módulo Mando Apolo", "Apollo CSM Module",
          "Arquitectura lunar clásica con módulo de servicio y ablación a 11 km/s.", "Classic lunar architecture with service module & 11 km/s heat shield.",
          180.0f, 14.0f, 0.0f, 0.0f, 0.0f, false },
        { 3, PartCategory::CAPSULE,
          "Cápsula Orion MPCV", "Orion MPCV Capsule",
          "Nave Artemis de espacio profundo con refugio antirradiación.", "Deep-space Artemis spacecraft with solar storm radiation shelter.",
          245.0f, 17.5f, 0.0f, 0.0f, 0.0f, false },
        { 4, PartCategory::CAPSULE,
          "Cápsula Dragon-Lunar", "Dragon-Lunar Capsule",
          "Diseño compuesto aerodinámico reutilizable de alta capacidad.", "High-capacity reusable composite aerodynamic spacecraft.",
          290.0f, 16.0f, 0.0f, 0.0f, 0.0f, false },
        { 5, PartCategory::CAPSULE,
          "Módulo Deep-Space", "Deep-Space Habitat",
          "Módulo presurizado avanzado para estancias lunares prolongadas.", "Advanced pressurized module for extended lunar surface missions.",
          350.0f, 21.0f, 0.0f, 0.0f, 0.0f, false },

        // ==================== 6..11: MÓDULOS LUNARES ====================
        { 6, PartCategory::LANDER,
          "Sonda Impacto Ranger", "Ranger Impact Probe",
          "Sonda no tripulada ligera para reconocimiento inicial.", "Lightweight unmanned probe for initial lunar reconnaissance.",
          70.0f, 5.0f, 0.0f, 0.0f, 0.0f, true },
        { 7, PartCategory::LANDER,
          "Aterrizador Surveyor", "Surveyor Lander",
          "Módulo automático ligero con radar Doppler de descenso.", "Light automated lander with Doppler descent radar.",
          135.0f, 8.5f, 0.0f, 0.0f, 0.0f, false },
        { 8, PartCategory::LANDER,
          "Módulo Lunar Eagle", "Eagle Lunar Module",
          "Aterrizador tripulado bietapa con lámina térmica Mylar dorada.", "Two-stage crewed lander wrapped in gold Mylar thermal foil.",
          220.0f, 15.0f, 0.0f, 0.0f, 0.0f, false },
        { 9, PartCategory::LANDER,
          "Aterrizador Altair", "Altair Heavy Lander",
          "Módulo lunar de cuatro patas con LIDAR de evasión de cráteres.", "Four-legged lunar lander with LIDAR crater hazard avoidance.",
          295.0f, 20.0f, 0.0f, 0.0f, 0.0f, false },
        { 10, PartCategory::LANDER,
          "Módulo Blue-Moon", "Blue-Moon HLS Lander",
          "Aterrizador criogénico de precisión para el Polo Sur lunar.", "Precision cryogenic lander engineered for the lunar South Pole.",
          360.0f, 23.0f, 0.0f, 0.0f, 0.0f, false },
        { 11, PartCategory::LANDER,
          "Estación Base Móvil", "Mobile Surface Base",
          "Hábitat lunar presurizado con laboratorio científico integrado.", "Pressurized lunar surface habitat with integrated science lab.",
          440.0f, 28.0f, 0.0f, 0.0f, 0.0f, false },

        // ==================== 12..17: TANQUES DE COMBUSTIBLE ====================
        { 12, PartCategory::TANK,
          "Tanque Redstone S1", "Redstone S1 Tank",
          "Depósito suborbital estrecho (95 t propelente). Insuficiente para la Luna.", "Narrow suborbital tank (95 t propellant). Too small for the Moon.",
          100.0f, 12.0f, 95.0f, 0.0f, 0.0f, true },
        { 13, PartCategory::TANK,
          "Tanque Titan-II", "Titan-II Core Tank",
          "Depósito orbital medio (210 t propelente) para órbita terrestre baja.", "Medium orbital tank (210 t propellant) suited for Low Earth Orbit.",
          190.0f, 20.0f, 210.0f, 0.0f, 0.0f, false },
        { 14, PartCategory::TANK,
          "Etapa S-IVB", "S-IVB Translunar Tank",
          "Tanque criogénico de hidrógeno líquido (340 t propelente).", "Cryogenic liquid hydrogen tank (340 t propellant).",
          285.0f, 28.0f, 340.0f, 0.0f, 0.0f, false },
        { 15, PartCategory::TANK,
          "Tanque Central SLS", "SLS Core Stage Tank",
          "Etapa central naranja criogénica Artemis (470 t propelente).", "Orange cryogenic Artemis core stage (470 t propellant).",
          380.0f, 36.0f, 470.0f, 0.0f, 0.0f, false },
        { 16, PartCategory::TANK,
          "Tanque Saturn S-IC", "Saturn S-IC Heavy Tank",
          "Coloso lunar de alta capacidad (640 t propelente RP-1/LOX).", "High-capacity lunar workhorse tank (640 t RP-1/LOX propellant).",
          480.0f, 46.0f, 640.0f, 0.0f, 0.0f, false },
        { 17, PartCategory::TANK,
          "Super-Criogénico H2", "Super-Cryogenic H2 Tank",
          "Tanque ultrapesado Block 2 (820 t propelente de alta densidad).", "Ultra-heavy Block 2 tank (820 t high-density cryogenic propellant).",
          590.0f, 56.0f, 820.0f, 0.0f, 0.0f, false },

        // ==================== 18..23: MOTORES PRINCIPALES ====================
        { 18, PartCategory::ENGINE,
          "Motor RL-10", "RL-10 Engine",
          "Motor criogénico de etapa superior (1.650 kN | Isp 430s).", "Upper-stage cryogenic engine (1,650 kN | Isp 430s).",
          90.0f, 4.0f, 0.0f, 1650.0f, 430.0f, true },
        { 19, PartCategory::ENGINE,
          "Motor J-2", "Rocketdyne J-2",
          "Motor de hidrógeno líquido probado en Apolo (3.400 kN | Isp 421s).", "Apollo-proven liquid hydrogen engine (3,400 kN | Isp 421s).",
          175.0f, 7.5f, 0.0f, 3400.0f, 421.0f, false },
        { 20, PartCategory::ENGINE,
          "Cluster 3x RS-25", "3x RS-25 Cluster",
          "Tres motores principales criogénicos de alta eficiencia (5.580 kN).", "Three high-efficiency cryogenic main engines (5,580 kN).",
          270.0f, 11.0f, 0.0f, 5580.0f, 418.0f, false },
        { 21, PartCategory::ENGINE,
          "Cluster 4x RS-25", "4x RS-25 SLS Cluster",
          "Configuración Artemis SLS de cuatro toberas (7.440 kN | Isp 415s).", "Four-nozzle Artemis SLS configuration (7,440 kN | Isp 415s).",
          360.0f, 14.0f, 0.0f, 7440.0f, 415.0f, false },
        { 22, PartCategory::ENGINE,
          "Cluster 5x F-1", "5x F-1 Saturn Cluster",
          "Cinco motores gigantes Saturno V de empuje masivo (11.200 kN).", "Five massive Saturn V F-1 engines (11,200 kN thrust).",
          465.0f, 21.0f, 0.0f, 11200.0f, 365.0f, false },
        { 23, PartCategory::ENGINE,
          "Motor F-1B Avanzado", "Advanced F-1B Cluster",
          "Propulsión de nueva generación carbono-cerámica (13.800 kN | Isp 395s).", "Next-gen carbon-ceramic propulsion (13,800 kN | Isp 395s).",
          570.0f, 23.0f, 0.0f, 13800.0f, 395.0f, false },

        // ==================== 24..29: PROPULSORES LATERALES ====================
        { 24, PartCategory::BOOSTER,
          "Sin Propulsores", "No Side Boosters",
          "Configuración limpia sin cohetes aceleradores laterales.", "Clean configuration without lateral strap-on boosters.",
          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true },
        { 25, PartCategory::BOOSTER,
          "2x Castor-30", "2x Castor-30 Boosters",
          "Par de aceleradores sólidos ligeros (+2.400 kN | +75 t fuel).", "Pair of light solid boosters (+2,400 kN | +75 t fuel).",
          85.0f, 10.0f, 75.0f, 2400.0f, 270.0f, false },
        { 26, PartCategory::BOOSTER,
          "2x Gem-63XL", "2x Gem-63XL Boosters",
          "Aceleradores de fibra de carbono (+4.200 kN | +135 t fuel).", "Carbon-fiber strap-on boosters (+4,200 kN | +135 t fuel).",
          135.0f, 18.0f, 135.0f, 4200.0f, 274.0f, false },
        { 27, PartCategory::BOOSTER,
          "2x SRB 4-Segmentos", "2x 4-Segment SRBs",
          "Propulsores laterales clásicos (+6.800 kN | +220 t fuel).", "Classic shuttle-derived solid boosters (+6,800 kN | +220 t fuel).",
          185.0f, 28.0f, 220.0f, 6800.0f, 275.0f, false },
        { 28, PartCategory::BOOSTER,
          "2x SRB 5-Segmentos", "2x 5-Segment Artemis SRBs",
          "Propulsores pesados Artemis SLS (+9.600 kN | +310 t fuel).", "Heavy Artemis SLS solid boosters (+9,600 kN | +310 t fuel).",
          255.0f, 38.0f, 310.0f, 9600.0f, 278.0f, false },
        { 29, PartCategory::BOOSTER,
          "2x BOLE Compuesto", "2x BOLE Composite SRBs",
          "Aceleradores SLS Block 2 de máximo empuje (+12.400 kN | +390 t fuel).", "Maximum-thrust SLS Block 2 boosters (+12,400 kN | +390 t fuel).",
          330.0f, 44.0f, 390.0f, 12400.0f, 285.0f, false }
    };
}

void LabShopPhase::Init() {
    BuildCatalog();

    // Presupuesto ajustado: las piezas del cohete cuestan bastante más que en el catálogo base
    // (redondeado a múltiplos de 5). Cambia kPartPriceScale para afinar la dificultad económica.
    for (auto& item : catalog) {
        item.costMillions = std::round(item.costMillions * kPartPriceScale / 5.0f) * 5.0f;
    }

    if (!texturesLoaded) {
        // Carga exacta de Laboratory.png y Angar.png
        // lab_bg.png y hangar_bg.png son PNG de verdad (los originales eran JPG, que este raylib no lee)
        texLaboratory = LoadExactProjectImage("lab_bg.png");
        texAngar      = LoadExactProjectImage("hangar_bg.png");
        if (texLaboratory.id <= 0) texLaboratory = LoadExactProjectImage("Laboratory.png");
        if (texAngar.id <= 0)      texAngar      = LoadExactProjectImage("Angar.png");

        for (int i = 0; i < 6; ++i) {
            texCapsules[i] = LoadExactProjectImage(TextFormat("cap_%d.png", i + 1));
            texLanders[i]  = LoadExactProjectImage(TextFormat("lander_%d.png", i + 1));
            texTanks[i]    = LoadExactProjectImage(TextFormat("tank_%d.png", i + 1));
            texEngines[i]  = LoadExactProjectImage(TextFormat("eng_%d.png", i + 1));
        }

        for (int i = 0; i < 5; ++i) {
            texBoosters[i] = LoadExactProjectImage(TextFormat("boost_%d.png", i + 1));
        }

        texScientist = LoadExactProjectImage("Science02.png");
        for (int i = 0; i < 3; ++i) {
            texFinalRocket[i] = LoadExactProjectImage(TextFormat("rockets/cohete_%d.png", i + 1));
        }

        texturesLoaded = true;
    }
}

void LabShopPhase::ResetOwnershipToStarter() {
    for (auto& item : catalog) {
        item.owned = (item.id == 0 || item.id == 6 || item.id == 12 || item.id == 18 || item.id == 25);
    }
}

void LabShopPhase::OnEnter(SharedMissionData& data) {
    if (catalog.empty() || !texturesLoaded) Init();

    ResetOwnershipToStarter();
    activeCategory = PartCategory::CAPSULE;
    backpackTab = false;
    saveNoticeTimer = 0.0f;
    alertTimer = 0.0f;
    introActive = true;   // primero el laboratorio con la científica, luego el hangar
    buildTimer = -1.0f;
    introLine = 0;
    introChars = 0.0f;

    data.eqCapsuleIdx = 0;
    data.eqLanderIdx  = 6;
    data.eqTankIdx    = 12;
    data.eqEngineIdx  = 18;
    data.eqBoosterIdx = 25;
    data.backpack.clear();

    RecalculateRocket(data);
    data.remainingBudget = std::max(0.0f, data.awardedBudget - data.rocket.totalCost);
}

void LabShopPhase::RestoreFromSave(SharedMissionData& data) {
    if (catalog.empty() || !texturesLoaded) Init();

    ResetOwnershipToStarter();
    introActive = false;
    buildTimer = -1.0f;

    data.eqCapsuleIdx = std::clamp(data.eqCapsuleIdx, 0, 5);
    data.eqLanderIdx  = std::clamp(data.eqLanderIdx, 6, 11);
    data.eqTankIdx    = std::clamp(data.eqTankIdx, 12, 17);
    data.eqEngineIdx  = std::clamp(data.eqEngineIdx, 18, 23);
    data.eqBoosterIdx = std::clamp(data.eqBoosterIdx, 24, 29);

    // Partidas guardadas con piezas que ya no se ofrecen: se pasa a la más parecida
    data.eqCapsuleIdx = SnapToAvailable(0, data.eqCapsuleIdx);
    data.eqLanderIdx  = SnapToAvailable(1, data.eqLanderIdx);
    data.eqTankIdx    = SnapToAvailable(2, data.eqTankIdx);
    data.eqEngineIdx  = SnapToAvailable(3, data.eqEngineIdx);
    data.eqBoosterIdx = SnapToAvailable(4, data.eqBoosterIdx);

    // Solo se conservan provisiones que existan en el catálogo
    data.backpack.erase(std::remove_if(data.backpack.begin(), data.backpack.end(),
        [](const std::string& id) { return FindBackpackItem(id) == nullptr; }), data.backpack.end());

    catalog[data.eqCapsuleIdx].owned = true;
    catalog[data.eqLanderIdx].owned  = true;
    catalog[data.eqTankIdx].owned    = true;
    catalog[data.eqEngineIdx].owned  = true;
    catalog[data.eqBoosterIdx].owned = true;

    RecalculateRocket(data);
    data.remainingBudget = std::max(0.0f, data.awardedBudget - data.rocket.totalCost - BackpackCost(data.backpack));
}

void LabShopPhase::RecalculateRocket(SharedMissionData& data) {
    const ShopItem& cap = catalog[std::clamp(data.eqCapsuleIdx, 0, 5)];
    const ShopItem& lan = catalog[std::clamp(data.eqLanderIdx, 6, 11)];
    const ShopItem& tnk = catalog[std::clamp(data.eqTankIdx, 12, 17)];
    const ShopItem& eng = catalog[std::clamp(data.eqEngineIdx, 18, 23)];
    const ShopItem& bst = catalog[std::clamp(data.eqBoosterIdx, 24, 29)];

    data.rocket.capsule.name = Tr(data.lang, cap.nameEs.c_str(), cap.nameEn.c_str());
    data.rocket.lander.name  = Tr(data.lang, lan.nameEs.c_str(), lan.nameEn.c_str());
    data.rocket.tank.name    = Tr(data.lang, tnk.nameEs.c_str(), tnk.nameEn.c_str());
    data.rocket.engine.name  = Tr(data.lang, eng.nameEs.c_str(), eng.nameEn.c_str());
    data.rocket.booster.name = Tr(data.lang, bst.nameEs.c_str(), bst.nameEn.c_str());

    data.rocket.totalCost   = cap.costMillions + lan.costMillions + tnk.costMillions + eng.costMillions + bst.costMillions;
    data.rocket.maxFuelTons = tnk.fuelTons + bst.fuelTons;

    float dryMass = cap.dryMassTons + lan.dryMassTons + tnk.dryMassTons + eng.dryMassTons + bst.dryMassTons;
    float wetMass = dryMass + data.rocket.maxFuelTons;
    float totalThrustKN = eng.thrustKN + bst.thrustKN;

    const float g0 = 9.80665f;
    data.rocket.twr = (wetMass > 0.0f) ? (totalThrustKN / (wetMass * g0)) : 0.0f;

    float weightedIsp = (totalThrustKN > 0.0f)
        ? ((eng.thrustKN * eng.isp + bst.thrustKN * bst.isp) / totalThrustKN)
        : eng.isp;

    data.rocket.deltaV = (dryMass > 0.0f && wetMass > dryMass)
        ? (weightedIsp * g0 * std::log(wetMass / dryMass))
        : 0.0f;
}

int LabShopPhase::GetEquippedIndexForCategory(const SharedMissionData& data, PartCategory cat) const {
    switch (cat) {
        case PartCategory::CAPSULE: return data.eqCapsuleIdx;
        case PartCategory::LANDER:  return data.eqLanderIdx;
        case PartCategory::TANK:    return data.eqTankIdx;
        case PartCategory::ENGINE:  return data.eqEngineIdx;
        case PartCategory::BOOSTER: return data.eqBoosterIdx;
    }
    return 0;
}

void LabShopPhase::SetEquippedIndexForCategory(SharedMissionData& data, PartCategory cat, int itemIdx) {
    switch (cat) {
        case PartCategory::CAPSULE: data.eqCapsuleIdx = itemIdx; break;
        case PartCategory::LANDER:  data.eqLanderIdx  = itemIdx; break;
        case PartCategory::TANK:    data.eqTankIdx    = itemIdx; break;
        case PartCategory::ENGINE:  data.eqEngineIdx  = itemIdx; break;
        case PartCategory::BOOSTER: data.eqBoosterIdx = itemIdx; break;
    }
}

GameState LabShopPhase::Update(SharedMissionData& data) {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();

    if (saveNoticeTimer > 0.0f) saveNoticeTimer = std::max(0.0f, saveNoticeTimer - dt);
    if (alertTimer > 0.0f)      alertTimer      = std::max(0.0f, alertTimer - dt);

    if (IsKeyPressed(KEY_M)) return GameState::MENU;

    if (buildTimer >= 0.0f) {
        // Montaje en marcha: martillazos mientras el humo tapa las piezas, fanfarria al descubrir el cohete
        float before = buildTimer;
        buildTimer += dt;
        if (buildTimer < kBuildSwap && static_cast<int>(before / 0.16f) != static_cast<int>(buildTimer / 0.16f)) {
            SfxTone(static_cast<float>(GetRandomValue(140, 320)), 0.08f, 'n', 0.06f);
        }
        if (before < kBuildSwap && buildTimer >= kBuildSwap) {
            SfxTone(523.0f, 0.12f); SfxTone(659.0f, 0.2f, 'q', 0.05f); SfxTone(784.0f, 0.34f, 'q', 0.05f);
        }
        if (buildTimer >= kBuildEnd) {
            buildTimer = -1.0f;
            return GameState::FLIGHT;
        }
        return GameState::HANGAR;
    }

    if (introActive) {
        // Texto letra a letra; clic / ESPACIO / ENTER lo completa y luego pasa a la siguiente frase
        std::string line = IntroLine(data, introLine);
        float total = static_cast<float>(line.size());
        size_t before = static_cast<size_t>(introChars);
        introChars = std::min(total, introChars + dt * 42.0f);
        size_t now = static_cast<size_t>(introChars);
        if (now > before && now <= line.size()) VoiceBlip(line[now - 1], VOICE_SCIENTIST);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (introChars < total) {
                introChars = total;
            } else if (introLine + 1 < kIntroLines) {
                ++introLine;
                introChars = 0.0f;
            } else {
                introActive = false;
            }
        }
        return GameState::HANGAR;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Rectangle btnMenu = { 24.0f, 14.0f, 105.0f, 34.0f };
        Rectangle btnSave = { 138.0f, 14.0f, 130.0f, 34.0f };

        if (CheckCollisionPointRec(mouse, btnMenu)) {
            return GameState::MENU;
        }
        if (CheckCollisionPointRec(mouse, btnSave)) {
            SaveManager::SaveGame(data.activeSaveSlot, data, GameState::HANGAR);
            saveNoticeTimer = 2.2f;
            return GameState::HANGAR;
        }

        // Pestañas: 4 categorías de piezas + MOCHILA
        for (int c = 0; c < kTabCount; ++c) {
            Rectangle tabBtn = { 385.0f + c * 106.0f, 76.0f, 100.0f, 36.0f };
            if (CheckCollisionPointRec(mouse, tabBtn)) {
                backpackTab = (c == 4);
                if (!backpackTab) activeCategory = static_cast<PartCategory>(kTabCategory[c]);
                return GameState::HANGAR;
            }
        }

        if (backpackTab) {
            // Provisiones: clic para meter en la mochila, clic otra vez para devolverla
            const auto& items = BackpackCatalog();
            for (int row = 0; row < static_cast<int>(items.size()); ++row) {
                Rectangle rowRect = { 385.0f, 124.0f + row * 49.2f, 524.0f, 45.0f };
                if (!CheckCollisionPointRec(mouse, rowRect)) continue;

                auto it = std::find(data.backpack.begin(), data.backpack.end(), std::string(items[row].id));
                if (it != data.backpack.end()) {
                    data.backpack.erase(it);
                    data.remainingBudget += items[row].costMillions;
                } else if (data.remainingBudget >= items[row].costMillions) {
                    data.backpack.push_back(items[row].id);
                    data.remainingBudget -= items[row].costMillions;
                } else {
                    alertMsgEs = "Presupuesto insuficiente para esta provisión.";
                    alertMsgEn = "Insufficient budget for this supply.";
                    alertTimer = 2.6f;
                }
                return GameState::HANGAR;
            }
        } else {
            // Lista de 3 piezas de la categoria seleccionada
            int cat = static_cast<int>(activeCategory);
            int currentEqIdx = GetEquippedIndexForCategory(data, activeCategory);
            float currentCatCost = catalog[currentEqIdx].costMillions;

            for (int row = 0; row < 3; ++row) {
                int idx = kAvail[cat][row];
                Rectangle cardRect = { 385.0f, 124.0f + row * 164.0f, 524.0f, 156.0f };
                if (CheckCollisionPointRec(mouse, cardRect)) {
                    if (idx == currentEqIdx) {
                        return GameState::HANGAR;
                    }

                    float netCost = catalog[idx].costMillions - currentCatCost;
                    if (data.remainingBudget >= netCost) {
                        data.remainingBudget -= netCost;
                        catalog[idx].owned = true;
                        SetEquippedIndexForCategory(data, activeCategory, idx);
                        RecalculateRocket(data);
                    } else {
                        alertMsgEs = "Presupuesto insuficiente para equipar esta pieza.";
                        alertMsgEn = "Insufficient budget to equip this component.";
                        alertTimer = 2.6f;
                    }
                    return GameState::HANGAR;
                }
            }
        }

        // Boton Lanzar a la Luna (Fase 3)
        Rectangle btnLaunch = { 932.0f, 628.0f, 324.0f, 58.0f };
        if (CheckCollisionPointRec(mouse, btnLaunch)) {
            RecalculateRocket(data);
            // si está el dibujo del cohete terminado, primero se ve cómo se construye
            if (texFinalRocket[RocketModelFromParts(data) - 1].id <= 0) return GameState::FLIGHT;
            buildTimer = 0.0f;
            return GameState::HANGAR;
        }
    }

    return GameState::HANGAR;
}

void LabShopPhase::DrawRocketPreview(const SharedMissionData& data, float centerX, float bottomY, float scale) const {
    int capIdx   = std::clamp(data.eqCapsuleIdx, 0, 5);
    int lanIdx   = std::clamp(data.eqLanderIdx - 6, 0, 5);
    int tnkIdx   = std::clamp(data.eqTankIdx - 12, 0, 5);
    int engIdx   = std::clamp(data.eqEngineIdx - 18, 0, 5);
    int boostIdx = (data.eqBoosterIdx >= 25) ? std::clamp(data.eqBoosterIdx - 25, 0, 4) : -1;

    bool hasModularPng = (texCapsules[capIdx].id > 0 || texTanks[tnkIdx].id > 0 || texEngines[engIdx].id > 0);
    if (hasModularPng) {
        // Cada sprite conserva su proporción: se fija el ancho y la altura sale de la imagen
        float curY = bottomY;
        auto drawPart = [&](Texture2D t, float partW) {
            if (t.id <= 0) return;
            float w = partW * scale;
            float hh = w * static_cast<float>(t.height) / static_cast<float>(t.width);
            DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                           { centerX - w * 0.5f, curY - hh, w, hh }, { 0, 0 }, 0.0f, WHITE);
            curY -= hh;
        };

        // Boosters laterales: un sprite a cada lado del tanque (el derecho, en espejo), por detrás del cohete
        if (boostIdx >= 0 && texBoosters[boostIdx].id > 0) {
            Texture2D tb = texBoosters[boostIdx];
            Texture2D te = texEngines[engIdx];
            float engH = (te.id > 0) ? kEngW * scale * static_cast<float>(te.height) / static_cast<float>(te.width) : 0.0f;
            float bw = kBoostW * scale;
            float bh = bw * static_cast<float>(tb.height) / static_cast<float>(tb.width);
            float baseY = bottomY - engH + 14.0f * scale;
            for (int side = -1; side <= 1; side += 2) {
                float bx = centerX + side * kBoostOffsetX * scale;
                DrawTexturePro(tb, { 0, 0, side * (float)tb.width, (float)tb.height },
                               { bx - bw * 0.5f, baseY - bh, bw, bh }, { 0, 0 }, 0.0f, WHITE);
            }
        }

        drawPart(texEngines[engIdx], kEngW);
        drawPart(texTanks[tnkIdx], kTankW);
        drawPart(texLanders[lanIdx], kTankW);
        drawPart(texCapsules[capIdx], kCapW);
        return;
    }

    float tankW = (50.0f + tnkIdx * 6.0f) * scale;
    float tankH = (135.0f + tnkIdx * 9.0f) * scale;
    float engH  = 36.0f * scale;
    float lanH  = 54.0f * scale;
    float capH  = 54.0f * scale;

    int nozzleCount = std::min(5, 1 + engIdx);
    float spread = (tankW * 0.72f) / std::max(1, nozzleCount);
    for (int n = 0; n < nozzleCount; ++n) {
        float nx = (nozzleCount == 1) ? centerX : (centerX - tankW * 0.32f + n * spread);
        DrawTriangle({ nx - 7.0f * scale, bottomY - engH },
                     { nx - 11.0f * scale, bottomY },
                     { nx + 11.0f * scale, bottomY }, Color{ 62, 70, 84, 255 });
        DrawTriangle({ nx - 7.0f * scale, bottomY - engH },
                     { nx + 11.0f * scale, bottomY },
                     { nx + 7.0f * scale, bottomY - engH }, Color{ 95, 105, 122, 255 });
    }

    float tankBot = bottomY - engH;
    float tankTop = tankBot - tankH;
    Color tankColor = (tnkIdx == 3 || tnkIdx == 5) ? Color{ 218, 108, 38, 255 } : Color{ 238, 242, 248, 255 };
    DrawRectangle(static_cast<int>(centerX - tankW * 0.5f), static_cast<int>(tankTop),
                  static_cast<int>(tankW), static_cast<int>(tankH), tankColor);
    DrawRectangleLines(static_cast<int>(centerX - tankW * 0.5f), static_cast<int>(tankTop),
                       static_cast<int>(tankW), static_cast<int>(tankH), Color{ 35, 45, 65, 255 });
    DrawRectangle(static_cast<int>(centerX - tankW * 0.5f), static_cast<int>(tankBot - 14 * scale),
                  static_cast<int>(tankW), static_cast<int>(14 * scale), Color{ 25, 30, 42, 255 });
    DrawRectangle(static_cast<int>(centerX - tankW * 0.5f), static_cast<int>(tankTop),
                  static_cast<int>(tankW), static_cast<int>(12 * scale), Color{ 25, 30, 42, 255 });

    float lanBot = tankTop;
    float lanTop = lanBot - lanH;
    float lanW = tankW * 0.86f;
    DrawRectangle(static_cast<int>(centerX - lanW * 0.5f), static_cast<int>(lanTop),
                  static_cast<int>(lanW), static_cast<int>(lanH), Color{ 225, 230, 238, 255 });
    DrawRectangleLines(static_cast<int>(centerX - lanW * 0.5f), static_cast<int>(lanTop),
                       static_cast<int>(lanW), static_cast<int>(lanH), DARKGRAY);
    DrawRectangle(static_cast<int>(centerX - lanW * 0.32f), static_cast<int>(lanTop + 12 * scale),
                  static_cast<int>(lanW * 0.64f), static_cast<int>(lanH - 22 * scale), Color{ 212, 168, 46, 255 });

    float capBot = lanTop;
    float capTop = capBot - capH;
    Color capColor = (capIdx <= 1) ? Color{ 62, 68, 78, 255 } : Color{ 242, 245, 250, 255 };
    DrawTriangle({ centerX - tankW * 0.42f, capBot },
                 { centerX + tankW * 0.42f, capBot },
                 { centerX, capTop }, capColor);
    DrawLineEx({ centerX - tankW * 0.42f, capBot }, { centerX, capTop }, 2.0f, DARKGRAY);
    DrawLineEx({ centerX + tankW * 0.42f, capBot }, { centerX, capTop }, 2.0f, DARKGRAY);
    DrawLineEx({ centerX, capTop }, { centerX, capTop - 26.0f * scale }, 3.0f, RED);

    if (boostIdx >= 0) {
        float bW = (16.0f + boostIdx * 2.2f) * scale;
        float bH = tankH * 0.88f;
        Color bCol = (boostIdx == 1 || boostIdx == 4) ? Color{ 48, 54, 66, 255 } : Color{ 245, 247, 252, 255 };
        for (int side = -1; side <= 1; side += 2) {
            float bx = centerX + side * (tankW * 0.5f + bW * 0.55f);
            DrawRectangle(static_cast<int>(bx - bW * 0.5f), static_cast<int>(tankBot - bH),
                          static_cast<int>(bW), static_cast<int>(bH), bCol);
            DrawRectangleLines(static_cast<int>(bx - bW * 0.5f), static_cast<int>(tankBot - bH),
                               static_cast<int>(bW), static_cast<int>(bH), BLACK);
            DrawTriangle({ bx - bW * 0.5f, tankBot - bH },
                         { bx + bW * 0.5f, tankBot - bH },
                         { bx, tankBot - bH - 20.0f * scale }, RED);
        }
    }
}

void LabShopPhase::Draw(const SharedMissionData& data) {
    Vector2 mouse = GetMousePosition();
    ClearBackground(Color{ 10, 15, 28, 255 });

    if (introActive) {
        DrawIntro(data);
        return;
    }

    // 1. FONDO: el hangar a pantalla completa (el laboratorio sale antes, en la intro)
    if (texAngar.id > 0) {
        DrawCover(texAngar, { 0.0f, 0.0f, (float)texAngar.width, (float)texAngar.height },
                  { 0.0f, 62.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT - 62.0f });
        DrawRectangle(0, 62, SCREEN_WIDTH, SCREEN_HEIGHT - 62, Color{ 8, 12, 24, 70 });
    }

    // Barra superior
    DrawRectangle(0, 0, SCREEN_WIDTH, 62, Color{ 10, 16, 32, 225 });
    DrawLine(0, 62, SCREEN_WIDTH, 62, GOLD);

    DrawRectangle(24, 14, 105, 34, Color{ 25, 35, 60, 245 });
    DrawRectangleLines(24, 14, 105, 34, GOLD);
    DrawUIText("MENÚ", 50.0f, 21.0f, 18.0f, WHITE);

    DrawRectangle(138, 14, 130, 34, Color{ 38, 125, 62, 245 });
    DrawRectangleLines(138, 14, 130, 34, GOLD);
    DrawUIText(Tr(data.lang, "GUARDAR", "SAVE"), 165.0f, 21.0f, 18.0f, WHITE);

    if (saveNoticeTimer > 0.0f) {
        DrawUIText(Tr(data.lang, "¡Partida guardada!", "Game saved!"), 282.0f, 21.0f, 18.0f, GREEN);
    } else {
        DrawUIText(Tr(data.lang, "FASE 2 · HANGAR DE ENSAMBLAJE", "PHASE 2 · ASSEMBLY HANGAR"),
                   290.0f, 20.0f, 20.0f, GOLD);
    }

    DrawUIText(TextFormat(Tr(data.lang,
        "FONDOS LIBRES: $%.0fM / $%.0fM",
        "AVAILABLE FUNDS: $%.0fM / $%.0fM"),
        data.remainingBudget, data.awardedBudget),
        885.0f, 20.0f, 21.0f, data.remainingBudget >= 0.0f ? GREEN : RED);

    // 2. COLUMNA IZQUIERDA DEL COHETE: FONDO CON Angar.png
    Rectangle hangarBox = { 24.0f, 76.0f, 345.0f, 622.0f };
    if (texAngar.id <= 0) DrawRectangleRec(hangarBox, Color{ 12, 18, 36, 220 });

    // Bandas semitransparentes arriba y abajo para leer bien los textos sobre Angar.png
    DrawRectangle(24, 76, 345, 44, Color{ 10, 15, 28, 195 });
    DrawRectangle(24, 602, 345, 96, Color{ 10, 15, 28, 205 });
    DrawRectangleLinesEx(hangarBox, 2.0f, SKYBLUE);

    DrawUIText(Tr(data.lang, "BAHÍA DE MONTAJE (ANGAR)", "ASSEMBLY BAY (ANGAR)"), 42.0f, 90.0f, 18.0f, GOLD);
    DrawLine(42, 118, 350, 118, Color{ 60, 85, 130, 220 });

    // Plataforma y cohete dibujados directamente delante de Angar.png
    DrawRectangle(95, 585, 202, 14, Color{ 70, 78, 95, 245 });
    bool building = (buildTimer >= 0.0f);
    int builtModel = RocketModelFromParts(data);
    if (building && buildTimer >= kBuildSwap) {
        DrawSpriteFit(texFinalRocket[builtModel - 1], 196.0f, 585.0f, 400.0f);
    } else {
        DrawRocketPreview(data, 196.0f, 585.0f, 1.02f);
    }
    if (building) {
        // Humo en bloques: crece hasta tapar el cohete y luego se abre hacia los lados y se deshace
        float t = buildTimer, tNow = static_cast<float>(GetTime());
        float cover = (t < kBuildSwap) ? std::min(1.0f, t / (kBuildSwap - 0.35f))
                                       : std::max(0.0f, 1.0f - (t - kBuildSwap - 0.25f) / 1.0f);
        cover = std::clamp(cover, 0.0f, 1.0f);
        float spread = (t > kBuildSwap + 0.25f) ? (t - kBuildSwap - 0.25f) * 110.0f : 0.0f;
        const Color smoke[3] = { { 182, 203, 208, 255 }, { 211, 235, 239, 255 }, { 245, 250, 252, 255 } };
        for (int layer = 0; layer < 3; ++layer) {
            for (int i = layer; i < 60; i += 3) {
                float h1 = std::sin(i * 12.9898f) * 43758.5453f; h1 -= std::floor(h1);
                float h2 = std::sin(i * 78.233f) * 12345.678f;   h2 -= std::floor(h2);
                float h3 = std::sin(i * 3.917f) * 9876.543f;     h3 -= std::floor(h3);
                float side = (h1 < 0.5f) ? -1.0f : 1.0f;
                float px = 196.0f + (h1 - 0.5f) * 250.0f + side * spread * (0.4f + h3);
                float py = 585.0f - h2 * 430.0f * std::min(1.0f, cover * 1.6f) + std::sin(tNow * 3.0f + i) * 5.0f;
                float sz = std::floor((18.0f + h3 * 58.0f) * cover / 4.0f) * 4.0f;
                if (sz < 4.0f) continue;
                Color c = smoke[layer];
                c.a = static_cast<unsigned char>(255.0f * std::min(1.0f, cover * 1.5f));
                DrawRectangle(static_cast<int>(std::floor((px - sz * 0.5f) / 4.0f) * 4.0f),
                              static_cast<int>(std::floor((py - sz * 0.5f) / 4.0f) * 4.0f),
                              static_cast<int>(sz), static_cast<int>(sz), c);
            }
        }
        // chispas y cartel cuando el cohete ya está montado
        if (t >= kBuildSwap) {
            for (int k = 0; k < 10; ++k) {
                float ph = std::fmod(t * 1.4f + k * 0.37f, 1.0f);
                float ang = k * 0.628f + 0.3f;
                float sx = 196.0f + std::cos(ang) * (60.0f + ph * 120.0f), sy = 380.0f + std::sin(ang) * (90.0f + ph * 160.0f);
                unsigned char a = static_cast<unsigned char>(255.0f * (1.0f - ph));
                DrawRectangle(static_cast<int>(sx) - 4, static_cast<int>(sy) - 4, 8, 8, Color{ 242, 193, 78, a });
            }
            const char* done = Tr(data.lang, "¡COHETE MONTADO!", "ROCKET BUILT!");
            float dw = MeasureUIText(done, 26.0f);
            DrawRectangle(static_cast<int>(196.0f - dw * 0.5f - 14.0f), 130, static_cast<int>(dw + 28.0f), 42, Color{ 34, 26, 52, 255 });
            DrawRectangle(static_cast<int>(196.0f - dw * 0.5f - 10.0f), 134, static_cast<int>(dw + 20.0f), 34, Color{ 12, 18, 34, 250 });
            DrawUIText(done, 196.0f - dw * 0.5f, 138.0f, 26.0f, builtModel == 1 ? GREEN : (builtModel == 2 ? GOLD : Color{ 225, 120, 60, 255 }));
        } else {
            const char* work = Tr(data.lang, "MONTANDO...", "BUILDING...");
            float ww = MeasureUIText(work, 24.0f);
            DrawRectangle(static_cast<int>(196.0f - ww * 0.5f - 14.0f), 130, static_cast<int>(ww + 28.0f), 40, Color{ 34, 26, 52, 255 });
            DrawRectangle(static_cast<int>(196.0f - ww * 0.5f - 10.0f), 134, static_cast<int>(ww + 20.0f), 32, Color{ 12, 18, 34, 250 });
            DrawUIText(work, 196.0f - ww * 0.5f, 138.0f, 24.0f, GOLD);
        }
    }

    // La científica junto al cohete y el modelo que saldrá a la plataforma con estas piezas
    DrawSpriteFit(texScientist, 326.0f, 600.0f, 104.0f, true);
    if (!building) {
        int model = RocketModelFromParts(data);
        Rectangle tag = { 262.0f, 126.0f, 100.0f, 150.0f };
        DrawRectangleRec(tag, Color{ 10, 15, 28, 205 });
        DrawRectangleLinesEx(tag, 1.5f, model == 1 ? GREEN : (model == 2 ? GOLD : Color{ 225, 120, 60, 255 }));
        const char* t1 = Tr(data.lang, "DESPEGARÁ", "WILL LAUNCH");
        DrawUIText(t1, tag.x + (tag.width - MeasureUIText(t1, 12.0f)) * 0.5f, tag.y + 5.0f, 12.0f, LIGHTGRAY);
        DrawSpriteFit(texFinalRocket[model - 1], tag.x + tag.width * 0.5f, tag.y + 124.0f, 100.0f);
        const char* t2 = TextFormat(Tr(data.lang, "COHETE %d", "ROCKET %d"), model);
        DrawUIText(t2, tag.x + (tag.width - MeasureUIText(t2, 13.0f)) * 0.5f, tag.y + 130.0f, 13.0f,
                   model == 1 ? GREEN : (model == 2 ? GOLD : Color{ 225, 120, 60, 255 }));
    }

    DrawUIText(TextFormat(Tr(data.lang, "Coste Total Cohete: $%.0fM", "Total Rocket Cost: $%.0fM"), data.rocket.totalCost),
               42.0f, 614.0f, 17.0f, WHITE);
    DrawUIText(TextFormat(Tr(data.lang, "Propelente Total: %.0f t", "Total Propellant: %.0f t"), data.rocket.maxFuelTons),
               42.0f, 640.0f, 17.0f, SKYBLUE);
    DrawUIText(Tr(data.lang, "Haz clic en piezas para equipar", "Click components to equip"),
               42.0f, 666.0f, 15.0f, LIGHTGRAY);

    // 3. COLUMNA CENTRAL: COMPRA DE PIEZAS (con Laboratory.png detras)
    const char* tabLabelsEs[5] = { "CÁPSULA", "TANQUE", "MOTOR", "BOOSTERS", "MOCHILA" };
    const char* tabLabelsEn[5] = { "CAPSULE", "TANK", "ENGINE", "BOOSTERS", "BACKPACK" };

    for (int c = 0; c < kTabCount; ++c) {
        Rectangle tabBtn = { 385.0f + c * 106.0f, 76.0f, 100.0f, 36.0f };
        bool selected = (c == 4) ? backpackTab : (!backpackTab && static_cast<int>(activeCategory) == kTabCategory[c]);
        DrawRectangleRec(tabBtn, selected ? Color{ 32, 68, 128, 235 } : Color{ 16, 24, 46, 200 });
        DrawRectangleLinesEx(tabBtn, 2.0f, selected ? GOLD : SKYBLUE);

        const char* lbl = Tr(data.lang, tabLabelsEs[c], tabLabelsEn[c]);
        float tw = MeasureUIText(lbl, 15.0f);
        DrawUIText(lbl, tabBtn.x + (tabBtn.width - tw) * 0.5f, tabBtn.y + 10.0f, 15.0f, selected ? GOLD : WHITE);
    }

    if (backpackTab) {
        // Provisiones para los problemas del viaje: 10 filas compactas
        const auto& items = BackpackCatalog();
        for (int row = 0; row < static_cast<int>(items.size()); ++row) {
            const BackpackItemDef& item = items[row];
            Rectangle rowRect = { 385.0f, 124.0f + row * 49.2f, 524.0f, 45.0f };

            bool packed = BackpackHas(data.backpack, item.id);
            bool hover = CheckCollisionPointRec(mouse, rowRect);
            bool canAfford = (data.remainingBudget >= item.costMillions);

            Color bgCol = packed
                ? Color{ 26, 68, 48, 210 }
                : (hover ? Color{ 26, 42, 78, 210 } : Color{ 14, 21, 40, 182 });
            Color borderCol = packed ? GREEN : (canAfford ? GOLD : Color{ 140, 60, 60, 220 });

            DrawRectangleRec(rowRect, bgCol);
            DrawRectangleLinesEx(rowRect, packed ? 2.5f : 1.5f, borderCol);

            DrawUIText(Tr(data.lang, item.nameEs, item.nameEn),
                       rowRect.x + 14.0f, rowRect.y + 5.0f, 17.0f, packed ? GREEN : WHITE);
            DrawUIText(Tr(data.lang, item.descEs, item.descEn),
                       rowRect.x + 14.0f, rowRect.y + 26.0f, 13.0f, LIGHTGRAY);

            if (packed) {
                DrawUIText(Tr(data.lang, "EN LA MOCHILA", "IN BACKPACK"), rowRect.x + 385.0f, rowRect.y + 6.0f, 14.0f, GREEN);
                DrawUIText(TextFormat("$%.0fM", item.costMillions), rowRect.x + 435.0f, rowRect.y + 25.0f, 13.0f, LIGHTGRAY);
            } else {
                DrawUIText(TextFormat("+$%.0fM", item.costMillions), rowRect.x + 430.0f, rowRect.y + 12.0f, 17.0f, canAfford ? GOLD : RED);
            }
        }
    } else {
        int cat = static_cast<int>(activeCategory);
        int currentEqIdx = GetEquippedIndexForCategory(data, activeCategory);
        float currentCatCost = catalog[currentEqIdx].costMillions;

        for (int row = 0; row < 3; ++row) {
            int idx = kAvail[cat][row];
            const ShopItem& item = catalog[idx];
            Rectangle cardRect = { 385.0f, 124.0f + row * 164.0f, 524.0f, 156.0f };

            bool equipped = (idx == currentEqIdx);
            bool hover = CheckCollisionPointRec(mouse, cardRect);
            float netCost = item.costMillions - currentCatCost;
            bool canAfford = (data.remainingBudget >= netCost);

            // Tarjetas semitransparentes para que Laboratory.png se vea por detras
            Color bgCol = equipped
                ? Color{ 26, 68, 48, 210 }
                : (hover ? Color{ 26, 42, 78, 210 } : Color{ 14, 21, 40, 182 });
            Color borderCol = equipped ? GREEN : (canAfford ? GOLD : Color{ 140, 60, 60, 220 });

            DrawRectangleRec(cardRect, bgCol);
            DrawRectangleLinesEx(cardRect, equipped ? 2.5f : 1.5f, borderCol);

            // Miniatura con el sprite de la pieza
            Rectangle thumb = { cardRect.x + 10.0f, cardRect.y + 10.0f, 112.0f, 136.0f };
            DrawRectangleRec(thumb, Color{ 8, 12, 24, 150 });
            Texture2D partTex{};
            if (cat == 0)      partTex = texCapsules[std::clamp(idx, 0, 5)];
            else if (cat == 2) partTex = texTanks[std::clamp(idx - 12, 0, 5)];
            else if (cat == 3) partTex = texEngines[std::clamp(idx - 18, 0, 5)];
            else if (cat == 4) partTex = texBoosters[std::clamp(idx - 25, 0, 4)];
            if (partTex.id > 0) {
                float fit = std::min(thumb.width * 0.88f / partTex.width, thumb.height * 0.88f / partTex.height);
                float pw = partTex.width * fit;
                float ph = partTex.height * fit;
                DrawTexturePro(partTex, { 0, 0, (float)partTex.width, (float)partTex.height },
                               { thumb.x + (thumb.width - pw) * 0.5f, thumb.y + (thumb.height - ph) * 0.5f, pw, ph },
                               { 0, 0 }, 0.0f, WHITE);
            }

            DrawUIText(Tr(data.lang, item.nameEs.c_str(), item.nameEn.c_str()),
                       cardRect.x + 136.0f, cardRect.y + 14.0f, 19.0f, equipped ? GREEN : WHITE);
            DrawUITextBoxed(Tr(data.lang, item.descEs.c_str(), item.descEn.c_str()),
                            { cardRect.x + 136.0f, cardRect.y + 46.0f, 268.0f, 96.0f }, 14.0f, 3.0f, LIGHTGRAY);

            if (equipped) {
                DrawUIText(Tr(data.lang, "EQUIPADO", "EQUIPPED"), cardRect.x + 415.0f, cardRect.y + 16.0f, 16.0f, GREEN);
                DrawUIText(TextFormat("$%.0fM", item.costMillions), cardRect.x + 435.0f, cardRect.y + 44.0f, 15.0f, LIGHTGRAY);
            } else {
                const char* diffTxt = (netCost >= 0.0f)
                    ? TextFormat("+$%.0fM", netCost)
                    : TextFormat("-$%.0fM", -netCost);
                DrawUIText(diffTxt, cardRect.x + 425.0f, cardRect.y + 16.0f, 17.0f, canAfford ? GOLD : RED);
                DrawUIText(TextFormat("($%.0fM)", item.costMillions), cardRect.x + 425.0f, cardRect.y + 44.0f, 14.0f, LIGHTGRAY);
            }
        }
    }

    if (alertTimer > 0.0f) {
        DrawRectangle(385, 624, 524, 38, Color{ 145, 32, 32, 245 });
        DrawRectangleLines(385, 624, 524, 38, WHITE);
        DrawUIText(Tr(data.lang, alertMsgEs.c_str(), alertMsgEn.c_str()), 402.0f, 633.0f, 17.0f, WHITE);
    }

    // 4. COLUMNA DERECHA: TELEMETRÍA Y LANZAMIENTO
    DrawRectangle(925, 76, 338, 622, Color{ 12, 18, 36, 195 });
    DrawRectangleLinesEx({ 925.0f, 76.0f, 338.0f, 622.0f }, 2.0f, GOLD);

    DrawUIText(Tr(data.lang, "TELEMETRÍA DE INGENIERÍA", "ENGINEERING TELEMETRY"), 942.0f, 90.0f, 18.0f, GOLD);
    DrawLine(942, 118, 1246, 118, Color{ 60, 85, 130, 220 });

    DrawUIText(Tr(data.lang, "CONFIGURACIÓN ACTUAL:", "CURRENT CONFIGURATION:"), 942.0f, 130.0f, 15.0f, SKYBLUE);
    DrawUIText(TextFormat("1. %s", data.rocket.capsule.name.c_str()), 942.0f, 154.0f, 15.0f, WHITE);
    DrawUIText(TextFormat("2. %s", data.rocket.lander.name.c_str()),  942.0f, 176.0f, 15.0f, WHITE);
    DrawUIText(TextFormat("3. %s", data.rocket.tank.name.c_str()),    942.0f, 198.0f, 15.0f, WHITE);
    DrawUIText(TextFormat("4. %s", data.rocket.engine.name.c_str()),  942.0f, 220.0f, 15.0f, WHITE);
    DrawUIText(TextFormat("5. %s", data.rocket.booster.name.c_str()), 942.0f, 242.0f, 15.0f, WHITE);

    DrawLine(942, 272, 1246, 272, Color{ 60, 85, 130, 220 });

    bool twrOk = (data.rocket.twr >= 1.20f);
    bool twrMin = (data.rocket.twr >= 1.02f);
    Color twrCol = twrOk ? GREEN : (twrMin ? GOLD : RED);
    DrawUIText(Tr(data.lang, "EMPUJE-PESO (TWR):", "THRUST-TO-WEIGHT (TWR):"), 942.0f, 286.0f, 16.0f, LIGHTGRAY);
    DrawUIText(TextFormat("%.2f  (Mínimo: 1,20)", data.rocket.twr), 942.0f, 310.0f, 22.0f, twrCol);

    bool dvOk = (data.rocket.deltaV >= 5800.0f);
    Color dvCol = dvOk ? GREEN : Color{ 255, 110, 110, 255 };
    DrawUIText(Tr(data.lang, "DELTA-V TOTAL (TSIOLKOVSKY):", "TOTAL DELTA-V (TSIOLKOVSKY):"), 942.0f, 354.0f, 16.0f, LIGHTGRAY);
    DrawUIText(TextFormat("%.0f m/s", data.rocket.deltaV), 942.0f, 378.0f, 24.0f, dvCol);
    DrawUIText(Tr(data.lang, "Objetivo Lunar: >= 5.800 m/s", "Lunar Target: >= 5,800 m/s"), 942.0f, 408.0f, 15.0f, SKYBLUE);

    DrawLine(942, 438, 1246, 438, Color{ 60, 85, 130, 220 });

    DrawRectangle(940, 452, 308, 156, Color{ 18, 28, 52, 215 });
    DrawRectangleLines(940, 452, 308, 156, (twrOk && dvOk) ? GREEN : GOLD);

    if (twrOk && dvOk) {
        DrawUIText(Tr(data.lang, "ESTADO: LISTO PARA LA LUNA", "STATUS: LUNAR MISSION READY"), 952.0f, 464.0f, 16.0f, GREEN);
        DrawUITextBoxed(Tr(data.lang,
            "Tu cohete cumple todos los requisitos orbitales de la NASA. Pulsa el botón inferior para auto-guardar e iniciar la secuencia de despegue.",
            "Your rocket meets all NASA orbital flight requirements. Press the button below to auto-save and start the launch sequence."),
            { 952.0f, 492.0f, 284.0f, 105.0f }, 15.0f, 3.0f, WHITE);
    } else {
        DrawUIText(Tr(data.lang, "AVISO DE INGENIERÍA:", "ENGINEERING ADVISORY:"), 952.0f, 464.0f, 16.0f, GOLD);
        const char* warnTxt = !twrMin
            ? Tr(data.lang, "TWR < 1,02: El cohete pesa demasiado y no podrá despegar de la plataforma. Mejora motores o boosters.",
                            "TWR < 1.02: Rocket is too heavy to lift off the pad. Upgrade engines or add side boosters.")
            : Tr(data.lang, "Delta-V < 5.800 m/s: Llegarás a órbita pero te quedarás sin combustible antes de alcanzar la Luna.",
                            "Delta-V < 5,800 m/s: You will reach orbit but run out of fuel before reaching the Moon.");
        DrawUITextBoxed(warnTxt, { 952.0f, 492.0f, 284.0f, 105.0f }, 15.0f, 3.0f, LIGHTGRAY);
    }

    Rectangle btnLaunch = { 932.0f, 628.0f, 324.0f, 58.0f };
    bool hoverLaunch = CheckCollisionPointRec(mouse, btnLaunch);
    DrawRectangleRec(btnLaunch, (twrOk && dvOk)
        ? (hoverLaunch ? Color{ 48, 155, 78, 255 } : Color{ 36, 125, 62, 255 })
        : (hoverLaunch ? Color{ 195, 130, 35, 255 } : Color{ 165, 105, 24, 255 }));
    DrawRectangleLinesEx(btnLaunch, 2.0f, WHITE);

    const char* launchTxt = Tr(data.lang, "AUTO-GUARDAR E IR A LA LUNA >", "AUTO-SAVE & LAUNCH TO MOON >");
    float lw = MeasureUIText(launchTxt, 17.0f);
    DrawUIText(launchTxt, btnLaunch.x + (btnLaunch.width - lw) * 0.5f, btnLaunch.y + 20.0f, 17.0f, WHITE);  
}

void LabShopPhase::DrawIntro(const SharedMissionData& data) const {
    // Laboratorio a pantalla completa
    if (texLaboratory.id > 0) {
        DrawCover(texLaboratory, { 0.0f, 0.0f, (float)texLaboratory.width, (float)texLaboratory.height },
                  { 0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT });
    }

    // La científica, con un pequeño balanceo mientras habla
    std::string line = IntroLine(data, introLine);
    bool talking = introChars < static_cast<float>(line.size());
    float bob = talking ? std::sin(static_cast<float>(GetTime()) * 14.0f) * 3.0f : 0.0f;
    DrawSpriteFit(texScientist, 600.0f, 508.0f + bob, 270.0f);

    DrawRectangle(0, 0, SCREEN_WIDTH, 62, Color{ 10, 16, 32, 225 });
    DrawLine(0, 62, SCREEN_WIDTH, 62, GOLD);
    DrawUIText(Tr(data.lang, "FASE 2 · LABORATORIO", "PHASE 2 · LABORATORY"), 24.0f, 20.0f, 20.0f, GOLD);

    // Caja de diálogo
    Rectangle box = { 140.0f, 540.0f, 1000.0f, 150.0f };
    DrawRectangleRec(box, Color{ 10, 15, 28, 240 });
    DrawRectangleLinesEx(box, 3.0f, GOLD);
    Rectangle name = { box.x + 24.0f, box.y - 20.0f, 190.0f, 36.0f };
    DrawRectangleRec(name, Color{ 32, 68, 128, 255 });
    DrawRectangleLinesEx(name, 2.0f, GOLD);
    const char* who = Tr(data.lang, "CIENTÍFICA", "SCIENTIST");
    DrawUIText(who, name.x + (name.width - MeasureUIText(who, 18.0f)) * 0.5f, name.y + 8.0f, 18.0f, WHITE);

    std::string shown = Utf8Prefix(line, static_cast<size_t>(introChars));
    DrawUITextBoxed(shown.c_str(), { box.x + 30.0f, box.y + 30.0f, box.width - 60.0f, 90.0f }, 24.0f, 8.0f, WHITE);

    if (!talking && std::fmod(static_cast<float>(GetTime()), 0.9f) < 0.55f) {
        const char* hint = Tr(data.lang, "CLIC o ESPACIO para continuar >", "CLICK or SPACE to continue >");
        DrawUIText(hint, box.x + box.width - MeasureUIText(hint, 15.0f) - 24.0f, box.y + box.height - 30.0f, 15.0f, GOLD);
    }
}

void LabShopPhase::Unload() {
    if (!texturesLoaded) return;

    if (texScientist.id > 0) { UnloadTexture(texScientist); texScientist.id = 0; }
    for (int i = 0; i < 3; ++i) {
        if (texFinalRocket[i].id > 0) { UnloadTexture(texFinalRocket[i]); texFinalRocket[i].id = 0; }
    }

    if (texLaboratory.id > 0) {
        UnloadTexture(texLaboratory);
        texLaboratory.id = 0;
    }
    if (texAngar.id > 0) {
        UnloadTexture(texAngar);
        texAngar.id = 0;
    }

    for (int i = 0; i < 6; ++i) {
        if (texCapsules[i].id > 0) UnloadTexture(texCapsules[i]);
        if (texLanders[i].id > 0)  UnloadTexture(texLanders[i]);
        if (texTanks[i].id > 0)    UnloadTexture(texTanks[i]);
        if (texEngines[i].id > 0)  UnloadTexture(texEngines[i]);
    }
    for (int i = 0; i < 5; ++i) {
        if (texBoosters[i].id > 0) UnloadTexture(texBoosters[i]);
    }

    texturesLoaded = false;
}