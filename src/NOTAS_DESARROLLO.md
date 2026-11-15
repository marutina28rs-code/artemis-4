# Unificación: problemas del viaje + mochila + sprites de piezas

Cambios hechos sobre el proyecto (copia de los archivos originales en `data/_backup_src/`).

## Archivos nuevos

- `src/incidents.h`, `src/incidents.cpp`: los problemas del viaje a la Luna (la fase 3 web pasada a raylib).
- `src/backpack_items.h`: catálogo de las 10 provisiones de la mochila.
- `assets/sprites/parts/`: sprites de las piezas (`cap_1/3/4`, `tank_1/3/4`, `eng_1/3/6`, `boost_1/3/4`).
  `tank_4.png` (tanque naranja) está generado a partir de vuestro tanque limpio, recoloreado.
- `assets/sprites/incidents/`, `assets/fonts/`: dibujos y tipografías de los problemas del viaje.
- `data/ai_config.txt`: clave de la IA (vacía).

## Archivos modificados

- `games_types.h`: `SharedMissionData::backpack` (ids de las provisiones compradas).
- `save_manager.cpp`: guarda y carga la mochila (línea `BACKPACK ...` al final; las partidas antiguas siguen cargando).
- `lab_shop.h/.cpp`:
  - 3 piezas por categoría, las que tienen sprite: cápsulas 0/2/3, tanques 12/14/15, motores 18/20/23, boosters 25/27/28.
  - La pestaña LANDER se sustituye por MOCHILA. El módulo lunar queda fijo en el de serie (no hay sprites de landers).
  - El cohete de serie lleva los boosters Castor-30 (antes "sin propulsores", que no tiene sprite).
  - Tarjetas con miniatura del sprite; el cohete montado respeta la proporción de cada imagen.
- `mission.cpp`:
  - Al completar la inyección translunar se llama a `RunIncidents(data)`. Con menos de 2 problemas resueltos la misión se aborta; si no, sigue el alunizaje.
  - `DrawModularRocket`: sprites con su proporción real y apilados sobre el eje del cohete.
  - `rotPoint` usaba el signo contrario al de las texturas (`angleDeg - 90` frente a `90 - angleDeg`), así que llamas y sprites giraban en sentidos opuestos. Ahora todo usa `90 - angleDeg`: el morro apunta a la derecha al inclinarse.
  - `Unload()` llama a `UnloadIncidents()`.

## Compilar

Hay que añadir `src/incidents.cpp` a la compilación. Si compiláis con `src/*.cpp` ya entra solo:

    g++ src/*.cpp -o moonshot.exe -O2 -std=c++17 -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm

El juego se ejecuta desde la carpeta raíz del proyecto (donde están `assets/` y `data/`).

## IA

`incidents.cpp` llama al programa `curl` (viene con Windows 10 y 11) en segundo plano, una vez por viaje.
Sin clave en `data/ai_config.txt`, o si la llamada falla, usa una tabla fija y el juego sigue igual.
No subáis `data/ai_config.txt` con la clave a GitHub.

## Pruebas hechas y no hechas

Comprobado fuera de Windows, con una biblioteca de pruebas que sustituye a raylib: compila sin errores,
la tienda mantiene bien el presupuesto, el guardado conserva la mochila y el viaje completo termina sin fallos
en español e inglés. No se ha compilado ni ejecutado con raylib real en Windows.

## Gamba (huevo de pascua) — fuentes de los datos curiosos

Los textos están en `src/shrimp.cpp`. Fuentes consultadas el 15-11-2026:

- Huevos de artemia en Apolo 16 y 17 (experimento Biostack): https://en.wikipedia.org/wiki/Biostack_experiment
- 12 personas han caminado sobre la Luna (1969-1972): https://www.britannica.com/story/what-have-we-left-on-the-moon
- Las huellas pueden durar millones de años: https://www.space.com/35352-eugene-cernan-moon-legacy-footprints.html
- SLS: 8,8 millones de libras de empuje, un 15 % más que el Saturno V; Artemis I despegó el 16-11-2022: https://www.space.com/nasa-artemis-1-sls-most-powerful-rocket y https://www.astronomy.com/space-exploration/artemis-1-nasa-sends-worlds-most-powerful-rocket-to-the-moon/
- Saturno V: 363 pies de alto: https://www.fox5dc.com/news/artemis-i-launch-sls-moon-rocket
- Alan Shepard y las pelotas de golf (Apolo 14): https://bigthink.com/?p=161884
- Artemisa, hermana gemela de Apolo, y Armstrong el 20-07-1969: datos de cultura general, sin fuente enlazada.

## Versión web (para Netlify)

El mismo código compila para navegador con Emscripten y raylib para web (PLATFORM_WEB). Los `#ifdef __EMSCRIPTEN__`
de `main.cpp` y `save_manager.cpp` solo hacen que las partidas se guarden en el navegador. `web_shell.html` es la
página que envuelve el juego. Comando usado (desde la carpeta del proyecto, con `libraylib.a` compilado para web):

    em++ -std=c++17 -O2 src/*.cpp -I<raylib>/src <raylib>/src/libraylib.a -DPLATFORM_WEB -sUSE_GLFW=3 -sASYNCIFY
         -sASYNCIFY_STACK_SIZE=262144 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -fexceptions
         --preload-file assets -lidbfs.js --shell-file src/web_shell.html -o dist/index.html

El resultado son 4 archivos (index.html, index.js, index.wasm, index.data): esa carpeta es la que se sube a Netlify.
En la versión web no hay IA (no hay `data/ai_config.txt`), así que los problemas del viaje usan la tabla fija.
