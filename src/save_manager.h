#pragma once

#include "games_types.h"
#include <string>

struct SaveSlotSummary {
    int slotNumber = 1;
    bool occupied = false;
    Language lang = Language::ES;
    GameState savedPhase = GameState::COMMITTEE;
    float awardedBudget = 0.0f;
    float remainingBudget = 0.0f;
    float dryMassTons = 0.0f;
    float thrustKN = 0.0f;
};

class SaveManager {
public:
    static bool SaveGame(int slot, const SharedMissionData& data, GameState currentPhase);
    static bool LoadGame(int slot, SharedMissionData& outData, GameState& outPhase);
    static SaveSlotSummary GetSlotSummary(int slot);
    static void DeleteSlot(int slot);

private:
    static std::string GetSlotFilePath(int slot);
};