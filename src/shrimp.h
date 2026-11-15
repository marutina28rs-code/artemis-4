#pragma once
// Huevo de pascua: una gamba que asoma de vez en cuando por un borde de la pantalla.
// Si se le hace clic, cuenta un dato curioso de la NASA.
#include "games_types.h"

void ShrimpInit();
void ShrimpUnload();
// Devuelve true si la gamba se ha quedado con la entrada de este fotograma (clic sobre ella o
// dato abierto): en ese caso el juego de debajo no debe actualizarse.
bool ShrimpUpdate(float dt, Vector2 mouse, Language lang);
void ShrimpDraw(Language lang);
