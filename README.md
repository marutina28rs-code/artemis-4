# ARTEMIS 4

Juego de diseño de misiones espaciales para el **NASA Space Apps Challenge 2026** (reto *Space Mission Design Game*).
Convence al comité, construye el cohete y llega a la Luna.

> **Jugar en el navegador:** `https://USUARIO.github.io/REPOSITORIO/` (sustituid por vuestra dirección de GitHub Pages)

## Cómo se juega

1. **Comité de presupuesto.** Respondes a las preguntas de tres jueces. Según cómo los convenzas, te dan más o menos
   presupuesto. Eliges dificultad (cadete, ingeniero o comandante), que también afecta al resto de la misión.
2. **Laboratorio y hangar.** Con ese presupuesto eliges cápsula, tanque, motor y boosters, y llenas la mochila de
   provisiones. El panel de telemetría calcula el empuje-peso (TWR) y el Delta-V con la ecuación del cohete: hacen
   falta TWR ≥ 1,20 y Delta-V ≥ 5.800 m/s. Según la calidad de las piezas despega uno de tres cohetes.
3. **Vuelo a la Luna.**
   - *Ascenso:* W/S acelerador, A/D inclinación, P piloto automático, ESPACIO velocidad ×2.
   - *Inyección translunar:* arrastra desde el cohete hacia atrás y suelta, como un tirachinas. La gravedad de la
     Tierra y de la Luna curva el tiro; hay que entrar en el anillo de la Luna esquivando asteroides.
   - *Problemas del viaje:* tres averías que se resuelven con minijuegos y con lo que lleves en la mochila.
   - *Alunizaje:* W o ESPACIO para frenar, A/D para centrarte en la zona verde.

Hay una gamba escondida que asoma de vez en cuando: si le haces clic, cuenta un dato curioso de la NASA.

## Contenido del repositorio

| Carpeta | Qué hay |
|---|---|
| `src/` | Código fuente en C++17 con [raylib](https://www.raylib.com/) |
| `assets/` | Dibujos pixel-art y tipografías del juego |
| `docs/` | Versión web ya compilada (WebAssembly), lista para GitHub Pages |

## Compilar

**Windows** (con raylib y w64devkit instalados en `C:\raylib`):

```
g++ src/*.cpp -o artemis4.exe -O2 -std=c++17 -static -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
```

El ejecutable tiene que estar junto a la carpeta `assets`.

**Web** (con Emscripten y raylib compilado para `PLATFORM_WEB`):

```
em++ -std=c++17 -O2 src/*.cpp -I<raylib>/src <raylib>/src/libraylib.a -DPLATFORM_WEB -sUSE_GLFW=3 -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -fexceptions --preload-file assets -lidbfs.js --shell-file src/web_shell.html -o docs/index.html
```

## Créditos

Claudia Sanz · Rafael Lizasoain · Gonzalo de Sagarra · Sol Gonzalez · Martina Ruipérez

- Motor: [raylib](https://www.raylib.com/) (licencia zlib).
- Tipografías: Press Start 2P y VT323 (SIL Open Font License; textos de licencia en `assets/fonts`).
- Fuentes de los datos curiosos: ver `src/NOTAS_DESARROLLO.md`.

## Uso de inteligencia artificial

Parte del código y de la integración se ha hecho con ayuda de un asistente de IA (Claude, de Anthropic).
