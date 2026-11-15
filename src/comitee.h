#pragma once

#include "games_types.h"
#include <string>
#include <vector>

struct CommitteeQuestion {
    std::string topicEs;
    std::string topicEn;
    std::string speakerEs;
    std::string speakerEn;
    std::string textEs;
    std::string textEn;
    std::string optionsEs[3];
    std::string optionsEn[3];
    std::string feedbackEs[3];
    std::string feedbackEn[3];
    float haleDelta[3];
    float brooksDelta[3];
    float vanceDelta[3];
};

class CommitteePhase {
public:
    void Init(Language lang);
    GameState Update(SharedMissionData& data);
    void Draw(const SharedMissionData& data);
    void Unload();

private:
    Texture2D texBg{};
    Texture2D texDesk{};
    Texture2D texHero{};
    Texture2D texGrey{};
    Texture2D texBlue{};
    Texture2D texSlick{};
    bool texturesLoaded = false;

    // 0 = intro, 1..8 = preguntas, 9 = deliberacion jueces, 10 = revelacion epica presupuesto
    int step = 0;
    int deliberationLine = 0;
    float epicRevealTimer = 0.0f;
    float displayedBudget = 0.0f;

    bool showNasaFacts = false;
    int nasaFactsPage = 0;

    float trustHale = 0.50f;
    float trustBrooks = 0.50f;
    float trustVance = 0.50f;

    int lastDeltaHale = 0;
    int lastDeltaBrooks = 0;
    int lastDeltaVance = 0;
    float deltaFlashTimer = 0.0f;

    std::string lastFeedbackEs;
    std::string lastFeedbackEn;
    float saveNoticeTimer = 0.0f;
    int voiceKey = -1;   // última frase a la que se le puso voz

    std::vector<CommitteeQuestion> questions;
};