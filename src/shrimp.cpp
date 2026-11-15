#include "shrimp.h"
#include "sfx.h"
#include <algorithm>
#include <cmath>

namespace {
    struct Fact { const char* es; const char* en; };
    // Datos comprobados (NASA y prensa especializada); ver LEEME para las fuentes
    const Fact kFacts[] = {
        { "Huevos de artemia, un camarón diminuto, viajaron a la Luna y volvieron en las misiones Apolo 16 y 17 para estudiar los rayos cósmicos. ¡Mis primos fueron astronautas!",
          "Eggs of brine shrimp flew to the Moon and back on Apollo 16 and 17 to study cosmic rays. My cousins were astronauts!" },
        { "Solo 12 personas han caminado sobre la Luna, todas entre 1969 y 1972.",
          "Only 12 people have walked on the Moon, all of them between 1969 and 1972." },
        { "En la Luna no hay viento ni lluvia, así que las huellas de los astronautas pueden durar millones de años.",
          "The Moon has no wind or rain, so the astronauts' footprints may last for millions of years." },
        { "El cohete SLS del programa Artemis despega con 8,8 millones de libras de empuje: un 15 % más que el Saturno V del Apolo.",
          "The SLS rocket of the Artemis program lifts off with 8.8 million pounds of thrust: 15% more than Apollo's Saturn V." },
        { "El Saturno V, el cohete que llevó a los astronautas del Apolo a la Luna, medía 363 pies de alto: unos 110 metros.",
          "The Saturn V, the rocket that took the Apollo astronauts to the Moon, stood 363 feet tall: about 110 metres." },
        { "Artemis I despegó el 16 de noviembre de 2022 y envió la nave Orion, sin tripulación, a dar la vuelta a la Luna.",
          "Artemis I lifted off on 16 November 2022 and sent the Orion spacecraft, with no crew, around the Moon." },
        { "En la misión Apolo 14, el astronauta Alan Shepard golpeó pelotas de golf sobre la Luna.",
          "On the Apollo 14 mission, astronaut Alan Shepard hit golf balls on the Moon." },
        { "En la mitología griega, Artemisa es la hermana gemela de Apolo. Por eso el programa que vuelve a la Luna se llama Artemis.",
          "In Greek mythology, Artemis is the twin sister of Apollo. That is why the program returning to the Moon is called Artemis." },
        { "Neil Armstrong pisó la Luna por primera vez el 20 de julio de 1969, en la misión Apolo 11.",
          "Neil Armstrong first set foot on the Moon on 20 July 1969, on the Apollo 11 mission." },
    };
    const int kFactCount = static_cast<int>(sizeof(kFacts) / sizeof(kFacts[0]));

    // Escondites: bordes de la pantalla donde no hay botones en ninguna fase
    struct Spot { float x, y; float dx, dy; bool mirror; };
    const Spot kSpots[] = {
        { 1250.0f, 704.0f, 0.0f, 1.0f, false },   // esquina inferior derecha, sube desde abajo
        { 1262.0f, 300.0f, 1.0f, 0.0f, false },   // borde derecho
        { 18.0f, 392.0f, -1.0f, 0.0f, true },     // borde izquierdo
    };
    const int kSpotCount = 3;
    const float kSize = 58.0f;

    Texture2D tex{};
    enum class State { HIDDEN, PEEK, FACT };
    State state = State::HIDDEN;
    float timer = 28.0f;       // segundos hasta la próxima aparición / tiempo asomada / tiempo del dato
    int spot = 0;
    int factIdx = -1;
    int order[16];
    int orderPos = kFactCount; // fuerza a barajar la primera vez

    float frand(float a, float b) { return a + (b - a) * (static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f); }

    Vector2 PeekPos() {
        // entra deslizándose desde fuera y se vuelve a esconder al final
        const Spot& s = kSpots[spot];
        float shown = 9.0f - timer;                       // tiempo asomada
        float in = std::clamp(shown / 0.6f, 0.0f, 1.0f);
        float out = std::clamp(timer / 0.6f, 0.0f, 1.0f);
        float k = 1.0f - std::min(in, out);               // 0 = asomada, 1 = escondida
        float wig = std::sin(static_cast<float>(GetTime()) * 5.0f) * 3.0f;
        return { s.x + s.dx * (kSize * k) + (s.dy != 0.0f ? wig : 0.0f),
                 s.y + s.dy * (kSize * k) + (s.dx != 0.0f ? wig : 0.0f) };
    }
}

void ShrimpInit() {
    if (tex.id > 0) return;
    const char* rel = "assets/sprites/gamba.png";
    if (FileExists(rel)) tex = LoadTexture(rel);
    else {
        std::string abs = std::string(GetApplicationDirectory()) + rel;
        if (FileExists(abs.c_str())) tex = LoadTexture(abs.c_str());
    }
    timer = frand(20.0f, 40.0f);
}

void ShrimpUnload() {
    if (tex.id > 0) UnloadTexture(tex);
    tex = Texture2D{};
}

bool ShrimpUpdate(float dt, Vector2 mouse, Language lang) {
    if (tex.id <= 0) return false;
    bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (state == State::HIDDEN) {
        timer -= dt;
        if (timer <= 0.0f) {
            state = State::PEEK;
            timer = 9.0f;
            spot = GetRandomValue(0, kSpotCount - 1);
        }
        return false;
    }

    if (state == State::PEEK) {
        timer -= dt;
        Vector2 p = PeekPos();
        if (click && std::hypot(mouse.x - p.x, mouse.y - p.y) < kSize * 0.75f) {
            if (orderPos >= kFactCount) {              // baraja: no se repite hasta haberlos visto todos
                for (int i = 0; i < kFactCount; ++i) order[i] = i;
                for (int i = kFactCount - 1; i > 0; --i) std::swap(order[i], order[GetRandomValue(0, i)]);
                if (factIdx < 0) { for (int i = 1; i < kFactCount; ++i) if (order[i] == 0) std::swap(order[0], order[i]); }
                orderPos = 0;
            }
            factIdx = order[orderPos++];
            state = State::FACT;
            timer = 0.0f;
            SfxTone(784.0f, 0.1f); SfxTone(1047.0f, 0.18f, 'q', 0.05f);
            VoiceSay(lang == Language::ES ? kFacts[factIdx].es : kFacts[factIdx].en, 520.0f, 12);
            return true;
        }
        if (timer <= 0.0f) { state = State::HIDDEN; timer = frand(45.0f, 90.0f); }
        return false;
    }

    // FACT: el juego queda en pausa hasta que se cierra con un clic o una tecla
    timer += dt;
    if (timer > 0.5f && (click || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
        state = State::HIDDEN;
        timer = frand(45.0f, 90.0f);
        VoiceStop();
    }
    return true;
}

void ShrimpDraw(Language lang) {
    if (tex.id <= 0 || state == State::HIDDEN) return;
    float aspect = static_cast<float>(tex.height) / static_cast<float>(tex.width);

    if (state == State::PEEK) {
        Vector2 p = PeekPos();
        float w = kSize, h = kSize * aspect;
        DrawTexturePro(tex, { 0, 0, (kSpots[spot].mirror ? -1.0f : 1.0f) * tex.width, static_cast<float>(tex.height) },
                       { p.x - w * 0.5f, p.y - h * 0.5f, w, h }, { 0, 0 }, 0.0f, WHITE);
        return;
    }

    const Color ink = { 34, 26, 52, 255 }, orange = { 232, 120, 60, 255 };
    float pop = std::min(1.0f, timer / 0.18f);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 6, 10, 20, static_cast<unsigned char>(150 * pop) });
    Rectangle box = { 250.0f, 250.0f + (1.0f - pop) * 30.0f, 780.0f, 220.0f };
    DrawRectangleRec({ box.x - 6.0f, box.y - 6.0f, box.width + 12.0f, box.height + 12.0f }, ink);
    DrawRectangleRec(box, orange);
    DrawRectangleRec({ box.x + 6.0f, box.y + 6.0f, box.width - 12.0f, box.height - 12.0f }, Color{ 13, 20, 38, 255 });

    float w = 150.0f, h = w * aspect;
    float bob = std::sin(static_cast<float>(GetTime()) * 6.0f) * 4.0f;
    DrawTexturePro(tex, { 0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height) },
                   { box.x + 28.0f, box.y + (box.height - h) * 0.5f + bob, w, h }, { 0, 0 }, 0.0f, WHITE);

    DrawUIText(lang == Language::ES ? "¡ME HAS ENCONTRADO! ¿SABÍAS QUE...?" : "YOU FOUND ME! DID YOU KNOW...?",
               box.x + 200.0f, box.y + 24.0f, 22.0f, Color{ 242, 193, 78, 255 });
    DrawUITextBoxed(lang == Language::ES ? kFacts[factIdx].es : kFacts[factIdx].en,
                    { box.x + 200.0f, box.y + 60.0f, box.width - 230.0f, 120.0f }, 21.0f, 6.0f, WHITE);
    if (timer > 0.5f && std::fmod(timer, 0.9f) < 0.55f) {
        const char* hint = lang == Language::ES ? "CLIC para seguir >" : "CLICK to continue >";
        DrawUIText(hint, box.x + box.width - MeasureUIText(hint, 16.0f) - 24.0f, box.y + box.height - 34.0f, 16.0f, Color{ 201, 208, 240, 255 });
    }
}
