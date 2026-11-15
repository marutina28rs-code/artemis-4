#include "save_manager.h"
#include <algorithm>
#include <fstream>
#include <filesystem>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Versión web: tras guardar o borrar, se vuelca la carpeta data al almacenamiento del navegador
static void WebSyncSaves() { EM_ASM( FS.syncfs(false, function (err) {}); ); }
#else
static void WebSyncSaves() {}
#endif

std::string SaveManager::GetSlotFilePath(int slot) {
    return "data/slot" + std::to_string(slot) + ".sav";
}

bool SaveManager::SaveGame(int slot, const SharedMissionData& data, GameState currentPhase) {
    if (slot < 1 || slot > 3) return false;

    std::error_code ec;
    std::filesystem::create_directories("data", ec);

    std::ofstream file(GetSlotFilePath(slot));
    if (!file.is_open()) return false;

    int phaseCode = 1;
    if (currentPhase == GameState::HANGAR) phaseCode = 2;
    else if (currentPhase == GameState::FLIGHT) phaseCode = 3;

    file << "MOONSHOT_SAVE_V1\n";
    file << (data.lang == Language::ES ? 0 : 1) << "\n";
    file << phaseCode << "\n";
    file << data.awardedBudget << "\n";
    file << data.remainingBudget << "\n";
    file << data.eqCapsuleIdx << "\n";
    file << data.eqLanderIdx << "\n";
    file << data.eqTankIdx << "\n";
    file << data.eqEngineIdx << "\n";
    file << data.eqBoosterIdx << "\n";
    file << data.rocket.totalCost << "\n";
    file << data.rocket.dryMassTons << "\n";
    file << data.rocket.maxFuelTons << "\n";
    file << data.rocket.thrustKN << "\n";
    file << data.rocket.twr << "\n";

    // Mochila de provisiones (línea opcional: las partidas antiguas no la tienen)
    file << "BACKPACK";
    for (const auto& id : data.backpack) file << " " << id;
    file << "\n";
    file << "DIFFICULTY " << data.difficulty << "\n";

    file.close();
    WebSyncSaves();
    return true;
}

bool SaveManager::LoadGame(int slot, SharedMissionData& outData, GameState& outPhase) {
    if (slot < 1 || slot > 3) return false;

    std::ifstream file(GetSlotFilePath(slot));
    if (!file.is_open()) return false;

    std::string header;
    std::getline(file, header);
    if (header != "MOONSHOT_SAVE_V1") return false;

    int langCode = 0;
    int phaseCode = 1;

    file >> langCode
         >> phaseCode
         >> outData.awardedBudget
         >> outData.remainingBudget
         >> outData.eqCapsuleIdx
         >> outData.eqLanderIdx
         >> outData.eqTankIdx
         >> outData.eqEngineIdx
         >> outData.eqBoosterIdx
         >> outData.rocket.totalCost
         >> outData.rocket.dryMassTons
         >> outData.rocket.maxFuelTons
         >> outData.rocket.thrustKN
         >> outData.rocket.twr;

    if (file.fail()) return false;

    outData.backpack.clear();
    std::string tag;
    if (file >> tag && tag == "BACKPACK") {
        std::string rest;
        std::getline(file, rest);
        std::istringstream ids(rest);
        std::string id;
        while (ids >> id) outData.backpack.push_back(id);
    }

    // Dificultad (línea opcional: las partidas antiguas no la tienen)
    outData.difficulty = 1;
    int diff = 1;
    if (file >> tag && tag == "DIFFICULTY" && (file >> diff)) outData.difficulty = std::clamp(diff, 0, 2);

    outData.activeSaveSlot = slot;
    outData.lang = (langCode == 0) ? Language::ES : Language::EN;

    if (phaseCode == 2) outPhase = GameState::HANGAR;
    else if (phaseCode == 3) outPhase = GameState::FLIGHT;
    else outPhase = GameState::COMMITTEE;

    outData.savedPhase = outPhase;
    return true;
}

SaveSlotSummary SaveManager::GetSlotSummary(int slot) {
    SaveSlotSummary summary;
    summary.slotNumber = slot;

    SharedMissionData temp;
    GameState phase = GameState::COMMITTEE;
    if (LoadGame(slot, temp, phase)) {
        summary.occupied = true;
        summary.lang = temp.lang;
        summary.savedPhase = phase;
        summary.awardedBudget = temp.awardedBudget;
        summary.remainingBudget = temp.remainingBudget;
        summary.dryMassTons = temp.rocket.dryMassTons;
        summary.thrustKN = temp.rocket.thrustKN;
    }
    return summary;
}

void SaveManager::DeleteSlot(int slot) {
    if (slot < 1 || slot > 3) return;
    std::error_code ec;
    std::filesystem::remove(GetSlotFilePath(slot), ec);
    WebSyncSaves();
}