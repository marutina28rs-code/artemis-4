#pragma once
// Problemas durante el viaje a la Luna (entre la inyección translunar y el alunizaje).
// Saltan tres problemas; en cada uno el jugador abre la mochila que compró en el hangar y elige
// con qué arreglarlo. Si resuelve al menos dos, la misión sigue hacia el alunizaje.
#include "games_types.h"
#include <string>

struct IncidentsResult {
    bool windowClosed = false; // el jugador cerró la ventana durante el viaje
    bool passed = false;       // resolvió al menos 2 problemas
    int solved = 0;
    int total = 0;
    std::string detailEs;      // resumen + consejo para el informe final
    std::string detailEn;
};

// Bloquea hasta que termina el viaje: dibuja sus propias pantallas (BeginDrawing/EndDrawing),
// así que hay que llamarla desde Update(), nunca entre BeginDrawing y EndDrawing.
IncidentsResult RunIncidents(const SharedMissionData& data);

// Libera texturas, fuentes y sonidos (llamar antes de CloseWindow).
void UnloadIncidents();
