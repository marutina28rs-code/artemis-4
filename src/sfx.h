#pragma once
// Sonido generado por código (sin archivos): voz "gibberish" de los personajes y efectos sencillos.
// La usan todas las fases para que el juego suene igual de principio a fin.
#include <string>

// Tonos base de cada personaje (Hz)
constexpr float VOICE_HERO      = 340.0f;   // la economista
constexpr float VOICE_HALE      = 150.0f;
constexpr float VOICE_BROOKS    = 185.0f;
constexpr float VOICE_VANCE     = 265.0f;
constexpr float VOICE_SCIENTIST = 300.0f;

void SfxInit();                      // abre el dispositivo de audio si hace falta
void SfxShutdown();                  // libera los sonidos (y cierra el audio si lo abrió SfxInit)
void SfxUpdate(float dt);            // llamar una vez por fotograma

// Un tono suelto: type 'q' cuadrada, 's' seno, 'w' sierra, 'n' ruido
void SfxTone(float freq, float dur = 0.08f, char type = 'q', float vol = 0.06f, float slide = 1.0f);

// Voz letra a letra para textos que se escriben poco a poco (limita sola la cadencia)
void VoiceBlip(char c, float basePitch);

// Voz para textos que aparecen de golpe: una ráfaga de sílabas proporcional a la longitud.
// Las ráfagas se encolan, así que dos personajes seguidos no se pisan.
void VoiceSay(const std::string& text, float basePitch, int maxBlips = 18);
void VoiceStop();
