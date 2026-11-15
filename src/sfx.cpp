#include "sfx.h"
#include "raylib.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <vector>

namespace {
    std::vector<Sound> live;
    bool audioOwned = false;
    double lastBlipAt = -1.0;

    struct Burst { std::string text; float pitch; int left; size_t pos; };
    std::deque<Burst> queue;
    float burstTimer = 0.0f;

    float frand(float a, float b) { return a + (b - a) * (static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f); }

    void blip(char c, float basePitch) {
        static const int steps[8] = { 0, 2, 4, 5, 7, 9, 7, 4 };
        int k = std::tolower(static_cast<unsigned char>(c)) % 8;
        SfxTone(basePitch * std::pow(2.0f, steps[k] / 12.0f) * frand(0.97f, 1.03f),
                0.07f, basePitch > 380.0f ? 's' : 'q', 0.05f, frand(0.9f, 1.15f));
    }
}

void SfxInit() {
    if (!IsAudioDeviceReady()) { InitAudioDevice(); audioOwned = true; }
}

void SfxShutdown() {
    for (auto& s : live) UnloadSound(s);
    live.clear();
    queue.clear();
    if (audioOwned && IsAudioDeviceReady()) CloseAudioDevice();
    audioOwned = false;
}

void SfxTone(float freq, float dur, char type, float vol, float slide) {
    if (!IsAudioDeviceReady()) return;
    for (size_t i = 0; i < live.size();) {
        if (!IsSoundPlaying(live[i])) { UnloadSound(live[i]); live.erase(live.begin() + static_cast<long>(i)); }
        else ++i;
    }
    if (live.size() > 24) return;

    const int rate = 44100;
    const int n = std::max(1, static_cast<int>(dur * rate));
    std::vector<short> smp(static_cast<size_t>(n));
    float ph = 0.0f;
    unsigned int noise = 0x1234567u + static_cast<unsigned int>(freq * 131.0f);
    float held = 0.0f;
    int holdLeft = 0;
    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        float f = std::max(20.0f, freq * std::pow(slide, t));
        ph += f / rate;
        float p = ph - std::floor(ph), v;
        if (type == 's') v = std::sin(p * 6.2831853f);
        else if (type == 'w') v = 2.0f * p - 1.0f;
        else if (type == 'n') {                      // ruido retenido: cuanto más baja la frecuencia, más grave
            if (holdLeft-- <= 0) {
                noise = noise * 1664525u + 1013904223u;
                held = static_cast<float>((noise >> 9) & 0xFFFF) / 32768.0f - 1.0f;
                holdLeft = std::max(1, static_cast<int>(rate / (f * 8.0f)));
            }
            v = held;
        }
        else v = (p < 0.5f) ? 1.0f : -1.0f;
        float env = std::min(1.0f, i / (rate * 0.008f)) * std::pow(1.0f - t, type == 'n' ? 1.2f : 2.2f);
        smp[static_cast<size_t>(i)] = static_cast<short>(std::clamp(v * env * vol * 3.2f, -1.0f, 1.0f) * 32767.0f * 0.9f);
    }
    Wave w{};
    w.frameCount = static_cast<unsigned int>(n);
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = smp.data();
    Sound s = LoadSoundFromWave(w);   // copia las muestras
    PlaySound(s);
    live.push_back(s);
}

void VoiceBlip(char c, float basePitch) {
    if (!std::isalnum(static_cast<unsigned char>(c))) return;
    double now = GetTime();
    if (now - lastBlipAt < 0.065) return;
    lastBlipAt = now;
    blip(c, basePitch);
}

void VoiceSay(const std::string& text, float basePitch, int maxBlips) {
    int letters = 0;
    for (unsigned char c : text) if (std::isalnum(c)) ++letters;
    if (letters == 0) return;
    int n = std::clamp(letters / 3, 3, maxBlips);
    if (queue.size() < 4) queue.push_back({ text, basePitch, n, 0 });
}

void VoiceStop() { queue.clear(); }

void SfxUpdate(float dt) {
    if (queue.empty()) return;
    burstTimer -= dt;
    if (burstTimer > 0.0f) return;
    Burst& b = queue.front();
    // siguiente letra del texto (cada tres, para que suene a sílabas)
    char c = 'a';
    for (int skip = 0; skip < 3 && b.pos < b.text.size(); ) {
        unsigned char ch = static_cast<unsigned char>(b.text[b.pos++]);
        if (std::isalnum(ch)) { c = static_cast<char>(ch); ++skip; }
    }
    if (b.pos >= b.text.size()) b.pos = 0;
    blip(c, b.pitch);
    burstTimer = frand(0.062f, 0.085f);
    if (--b.left <= 0) { queue.pop_front(); burstTimer = 0.16f; }
}
