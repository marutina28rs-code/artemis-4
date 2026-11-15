#include "comitee.h"
#include "sfx.h"
#include "save_manager.h"
#include <algorithm>
#include <cmath>

namespace {
    // 0 = Cadete (Facil), 1 = Ingeniero NASA (Normal), 2 = Comandante Apolo (Dificil)
    int s_selectedDifficulty = 1;

    struct DifficultyConfig {
        float startTrust;
        float gainMultiplier;
        float lossMultiplier;
        float baseBudget;
        float budgetSpan;
    };

    DifficultyConfig GetDifficultyConfig(int diff) {
        if (diff == 0) {
            return { 0.54f, 1.25f, 0.50f, 1100.0f, 1850.0f };
        }
        if (diff == 2) {
            return { 0.24f, 0.95f, 1.40f, 700.0f, 2050.0f };
        }
        return { 0.38f, 1.00f, 1.00f, 850.0f, 1900.0f };
    }
}

void CommitteePhase::Init(Language lang) {
    (void)lang;
    if (!texturesLoaded) {
        texBg    = LoadTexture("assets/sprites/bg.png");
        texDesk  = LoadTexture("assets/sprites/desk.png");
        texHero  = LoadTexture("assets/sprites/hero.png");
        texGrey  = LoadTexture("assets/sprites/grey.png");
        texBlue  = LoadTexture("assets/sprites/blue.png");
        texSlick = LoadTexture("assets/sprites/slick.png");
        texturesLoaded = true;
    }

    step = 0;
    deliberationLine = 0;
    voiceKey = -1;
    epicRevealTimer = 0.0f;
    displayedBudget = 0.0f;
    showNasaFacts = false;
    nasaFactsPage = 0;

    DifficultyConfig cfg = GetDifficultyConfig(s_selectedDifficulty);
    trustHale = cfg.startTrust;
    trustBrooks = cfg.startTrust;
    trustVance = cfg.startTrust;

    lastDeltaHale = 0;
    lastDeltaBrooks = 0;
    lastDeltaVance = 0;
    deltaFlashTimer = 0.0f;

    lastFeedbackEs.clear();
    lastFeedbackEn.clear();
    saveNoticeTimer = 0.0f;

    questions = {
        // 1 -> B
        {
            "1/12 · LÍNEA BASE FISCAL", "1/12 · FISCAL BASELINE",
            "SR. HALE (Subcomité de Asignaciones)", "MR. HALE (Appropriations Chair)",
            "El Congreso auditó sobrecostes previos. ¿Qué marco presupuestario plurianual solicita para garantizar el alunizaje?",
            "Congress audited prior overruns. What multi-year budget baseline do you request to guarantee a lunar landing?",
            {
                "A) $280.000M en cheque en blanco sin techo de gasto",
                "B) $93.000M auditados por hitos con reserva de contingencia del 15%",
                "C) $14.000M eliminando pruebas estáticas de motores en tierra"
            },
            {
                "A) $280B blank check with no spending cap",
                "B) $93B milestone-audited baseline with 15% contingency reserve",
                "C) $14B by eliminating ground static-fire engine tests"
            },
            {
                "Sr. Hale rechaza de plano un cheque sin auditoría ni techo fiscal.",
                "Sr. Hale asiente: una línea base realista auditada por hitos evita desvíos.",
                "Dra. Vance protesta: recortar pruebas en tierra multiplica el riesgo de explosión."
            },
            {
                "Mr. Hale flatly rejects an unaudited blank check.",
                "Mr. Hale nods: a realistic milestone-audited baseline prevents overruns.",
                "Dr. Vance objects: cutting ground tests multiplies explosion risk."
            },
            { -0.20f,  0.09f,  0.03f },
            { -0.14f,  0.07f, -0.14f },
            {  0.02f,  0.06f, -0.22f }
        },
        // 2 -> C
        {
            "2/12 · ARQUITECTURA ORBITAL", "2/12 · MISSION ARCHITECTURE",
            "DRA. VANCE (Ingeniera Jefe de Sistemas)", "DR. VANCE (Chief Systems Engineer)",
            "¿Qué perfil orbital emplearemos para minimizar la masa total que debe despegar desde Cabo Cañaveral?",
            "Which orbital flight profile will we use to minimize total launch mass from Cape Canaveral?",
            {
                "A) Ascenso Directo aterrizando todo el cohete superior en la Luna",
                "B) 14 lanzamientos independientes sin acoplamiento automatizado",
                "C) Encuentro en Órbita Lunar (LOR) con módulo de descenso dedicado"
            },
            {
                "A) Direct Ascent landing the entire upper rocket on the Moon",
                "B) 14 separate launches with no automated orbital docking",
                "C) Lunar Orbit Rendezvous (LOR) with dedicated lightweight lander"
            },
            {
                "Dra. Vance señala que el Ascenso Directo exige un supercohete inviable.",
                "Sr. Hale advierte que 14 lanzamientos disparan el coste operativo.",
                "Dra. Vance aprueba: LOR ahorra decenas de toneladas de propelente."
            },
            {
                "Dr. Vance notes Direct Ascent requires an unbuildable super-booster.",
                "Mr. Hale warns that 14 launches blow up operational costs.",
                "Dr. Vance approves: LOR saves dozens of tons of return propellant."
            },
            { -0.12f, -0.18f,  0.07f },
            { -0.08f, -0.14f,  0.06f },
            { -0.18f, -0.15f,  0.10f }
        },
        // 3 -> A
        {
            "3/12 · DINÁMICA DE DESPEGUE", "3/12 · LIFTOFF DYNAMICS",
            "DRA. VANCE (Ingeniera Jefe de Sistemas)", "DR. VANCE (Chief Systems Engineer)",
            "Con miles de toneladas en rampa, ¿qué relación Empuje-Peso (TWR) mínima exigirá al encender motores?",
            "With thousands of tons on the pad, what minimum Thrust-to-Weight Ratio (TWR) will you require at ignition?",
            {
                "A) TWR > 1,20 (como el 1,22 del Saturno V) para vencer pérdidas gravitatorias",
                "B) TWR = 1,00 exacto para estabilizar el cohete flotando antes de subir",
                "C) TWR = 0,85 para quemar combustible despacio y reducir presión"
            },
            {
                "A) TWR > 1.20 (like Saturn V's 1.22) to overcome gravity drag",
                "B) TWR = 1.00 exact so the rocket hovers before climbing",
                "C) TWR = 0.85 to burn fuel slowly and reduce dynamic pressure"
            },
            {
                "Dra. Vance valida el margen: superar 1,20 garantiza ascenso vertical limpio.",
                "Flotar a TWR 1,00 quema toneladas de propelente sin ganar un metro de altura.",
                "Con TWR < 1 el cohete ni siquiera se despegará de la plataforma."
            },
            {
                "Dr. Vance validates the margin: exceeding 1.20 ensures clean vertical climb.",
                "Hovering at TWR 1.00 wastes tons of propellant with zero altitude gain.",
                "With TWR < 1 the rocket won't even lift off the pad."
            },
            {  0.05f, -0.10f, -0.12f },
            {  0.05f, -0.08f, -0.10f },
            {  0.10f, -0.18f, -0.22f }
        },
        // 4 -> B
        {
            "4/12 · ECUACIÓN DE TSIOLKOVSKY", "4/12 · ROCKET EQUATION",
            "SR. HALE (Subcomité de Asignaciones)", "MR. HALE (Appropriations Chair)",
            "El Senado pregunta por qué los tanques ocupan casi toda la nave. ¿Qué fracción de masa en rampa es propelente?",
            "The Senate asks why tanks occupy nearly the entire vehicle. What pad mass fraction is propellant?",
            {
                "A) Un 40% de combustible y el 60% de blindaje estructural reforzado",
                "B) Entre el 85% y el 90% de la masa total (ley exponencial Delta-V)",
                "C) Menos del 20% gracias a la conservación del momento en el vacío"
            },
            {
                "A) 40% fuel and 60% heavy reinforced structural armor",
                "B) Between 85% and 90% of total pad mass (exponential Delta-V law)",
                "C) Under 20% thanks to momentum conservation in vacuum"
            },
            {
                "Dra. Vance corrige: con un 40% de propelente ni siquiera alcanzaríamos órbita baja.",
                "Sr. Hale entiende la física: lograr ~15 km/s de Delta-V exige esa ratio.",
                "El comité detecta que esa cifra viola la ecuación del cohete."
            },
            {
                "Dr. Vance corrects: 40% propellant wouldn't even reach Low Earth Orbit.",
                "Mr. Hale grasps the physics: reaching ~15 km/s Delta-V demands that ratio.",
                "The committee notes that figure violates the rocket equation."
            },
            { -0.14f,  0.09f, -0.16f },
            { -0.10f,  0.06f, -0.14f },
            { -0.18f,  0.08f, -0.20f }
        },
        // 5 -> C
        {
            "5/12 · PROPULSIÓN CRIOGÉNICA", "5/12 · CRYOGENIC PROPULSION",
            "DRA. VANCE (Ingeniera Jefe de Sistemas)", "DR. VANCE (Chief Systems Engineer)",
            "¿Cómo combinará propelentes entre la primera etapa y las etapas superiores translunares?",
            "How will you combine propellants between the first stage and upper translunar stages?",
            {
                "A) Queroseno RP-1 en todas las etapas por su mayor impulso específico",
                "B) Combustible sólido monolítico en la etapa de inyección translunar",
                "C) RP-1/LOX en 1ª etapa por empuje y LH2/LOX arriba por alto Isp (~450s)"
            },
            {
                "A) RP-1 kerosene across all stages for higher specific impulse",
                "B) Monolithic solid propellant for the translunar injection stage",
                "C) RP-1/LOX on Stage 1 for thrust & LH2/LOX upper stages for high Isp"
            },
            {
                "Dra. Vance corrige: el hidrógeno líquido tiene mucho mayor Isp que el RP-1.",
                "Los motores sólidos no pueden reencenderse ni modularse con precisión orbital.",
                "Dra. Vance asiente: densidad abajo y máxima eficiencia criogénica arriba."
            },
            {
                "Dr. Vance corrects: liquid hydrogen has far higher Isp than RP-1 kerosene.",
                "Solid motors cannot be restarted or throttled for precise orbital burns.",
                "Dr. Vance nods: high density at liftoff and peak cryogenic Isp upstairs."
            },
            { -0.06f, -0.08f,  0.06f },
            { -0.05f, -0.12f,  0.06f },
            { -0.16f, -0.18f,  0.10f }
        },
        // 6 -> A
        {
            "6/12 · SEGURIDAD DE TRIPULACIÓN", "6/12 · CREW SAFETY & ABORT",
            "PRESIDENTE BROOKS (Comité Espacial)", "CHAIRMAN BROOKS (Space Committee)",
            "Tras Apolo 1 y Challenger, ¿cómo garantizamos la supervivencia ante una anomalía crítica durante el ascenso?",
            "After Apollo 1 and Challenger, how do we ensure crew survival during a critical launch anomaly?",
            {
                "A) Torre de Escape (LES) sólida y modos de aborto intacto a órbita",
                "B) Eliminar la torre de escape para ahorrar 4,2 toneladas de masa seca",
                "C) Paracaídas individuales con apertura manual durante el ascenso"
            },
            {
                "A) Solid-rocket Launch Escape System (LES) and abort-to-orbit modes",
                "B) Remove the escape tower to save 4.2 tons of dry mass",
                "C) Individual parachutes with manual hatch exit during ascent"
            },
            {
                "Presidente Brooks respalda la seguridad tripulada como prioridad absoluta.",
                "Presidente Brooks rechaza recortar sistemas de salvamento tripulado.",
                "Dra. Vance señala que es imposible abrir escotillas en vuelo supersónico."
            },
            {
                "Chairman Brooks backs crew safety as an absolute non-negotiable priority.",
                "Chairman Brooks refuses to cut crew escape systems.",
                "Dr. Vance notes hatches cannot be opened during supersonic flight."
            },
            {  0.05f,  0.04f, -0.10f },
            {  0.10f, -0.22f, -0.20f },
            {  0.07f, -0.16f, -0.20f }
        },
        // 7 -> C
        {
            "7/12 · INYECCIÓN TRANSLUNAR", "7/12 · TRANSLUNAR INJECTION",
            "DRA. VANCE (Ingeniera Jefe de Sistemas)", "DR. VANCE (Chief Systems Engineer)",
            "Al encender la etapa TLI desde órbita terrestre, ¿hacia dónde apuntaremos el vector de velocidad?",
            "When firing the TLI stage from Earth orbit, where do we aim our velocity vector?",
            {
                "A) En línea recta hacia la posición actual donde vemos la Luna",
                "B) Rasante sobre la atmósfera lunar para frenar por rozamiento sin fuel",
                "C) Trayectoria de regreso libre apuntando 3 días por delante de la Luna"
            },
            {
                "A) Straight line directly at the Moon's current visible position",
                "B) Grazing the lunar atmosphere to aerocapture without propellant",
                "C) Free-return trajectory aimed 3 days ahead of the Moon's orbit"
            },
            {
                "En los 3 días de viaje la Luna se habrá desplazado miles de kilómetros.",
                "La Luna carece de atmósfera densa para frenar una nave a 2,4 km/s.",
                "Dra. Vance aprueba: interceptación precisa y retorno seguro si falla el motor."
            },
            {
                "Over the 3-day transit the Moon moves thousands of kilometers in orbit.",
                "The Moon lacks a dense atmosphere to aerocapture a craft at 2.4 km/s.",
                "Dr. Vance approves: accurate intercept plus safe Earth return if engines fail."
            },
            { -0.08f, -0.06f,  0.06f },
            { -0.08f, -0.08f,  0.08f },
            { -0.20f, -0.22f,  0.10f }
        },
        // 8 -> B
        {
            "8/12 · REENTRADA A 11 KM/S", "8/12 · 11 KM/S LUNAR REENTRY",
            "DRA. VANCE (Ingeniera Jefe de Sistemas)", "DR. VANCE (Chief Systems Engineer)",
            "Regresar de la Luna implica entrar en la atmósfera a ~11 km/s (4x la energía cinética de LEO). ¿Qué solución propone?",
            "Lunar return hits Earth's atmosphere at ~11 km/s (4x LEO kinetic energy). What thermal protection do you propose?",
            {
                "A) Losetas cerámicas finas diseñadas para órbita terrestre baja",
                "B) Escudo térmico ablativo fenólico y reentrada guiada de doble salto",
                "C) Frenar toda la velocidad orbital con retropropulsión antes de tocar aire"
            },
            {
                "A) Thin ceramic tiles rated for Low Earth Orbit shuttle reentry",
                "B) Phenolic ablative heat shield with guided skip-reentry profile",
                "C) Cancel all orbital velocity with retro-propulsion before hitting air"
            },
            {
                "Dra. Vance advierte que las losetas de LEO se fundirían a 11 km/s.",
                "Dra. Vance valida: la ablación disipa miles de grados y controla las fuerzas G.",
                "Frenar con motores exigiría llevar otro cohete entero hasta la Luna."
            },
            {
                "Dr. Vance warns LEO ceramic tiles would melt at 11 km/s reentry.",
                "Dr. Vance validates: ablation dissipates extreme heat and limits G-loads.",
                "Propulsive braking would require carrying another entire rocket to the Moon."
            },
            { -0.08f,  0.06f, -0.16f },
            { -0.14f,  0.07f, -0.10f },
            { -0.20f,  0.10f, -0.18f }
        },
        // 9 -> B
        {
            "9/12 · RADIACIÓN ESPACIO PROFUNDO", "9/12 · DEEP SPACE RADIATION",
            "PRESIDENTE BROOKS (Comité Espacial)", "CHAIRMAN BROOKS (Space Committee)",
            "Fuera del campo magnético terrestre, una tormenta solar es letal. ¿Cómo protegeremos tripulación y aviónica?",
            "Outside Earth's magnetic field, a solar storm is lethal. How will we protect crew and avionics?",
            {
                "A) Blindaje macizo de plomo de 15 cm alrededor de toda la cápsula",
                "B) Refugio rodeado de tanques de agua/víveres y ordenadores triples (TMR)",
                "C) Apagar toda la telemetría y sensores durante el cruce de Van Allen"
            },
            {
                "A) Solid 15 cm lead armor plating wrapped around the entire capsule",
                "B) Storm shelter buffered by water/provisions & triple-redundant computers",
                "C) Shut down all telemetry and sensors while crossing Van Allen belts"
            },
            {
                "El plomo añade toneladas prohibitivas y genera radiación secundaria.",
                "El comité elogia usar el agua potable como escudo y triple redundancia.",
                "Volar a ciegas sin navegación inercial ni telemetría es inaceptable."
            },
            {
                "Lead adds prohibitive mass and produces secondary bremsstrahlung radiation.",
                "The committee praises water shielding and triple-modular redundancy.",
                "Flying blind without inertial navigation or telemetry is unacceptable."
            },
            { -0.18f,  0.07f, -0.08f },
            { -0.06f,  0.10f, -0.18f },
            { -0.16f,  0.08f, -0.18f }
        },
        // 10 -> A
        {
            "10/12 · CONTROL DE CONTRATISTAS", "10/12 · CONTRACTOR GOVERNANCE",
            "SR. HALE (Subcomité de Asignaciones)", "MR. HALE (Appropriations Chair)",
            "¿Qué modelo contractual aplicará a los proveedores aeroespaciales para evitar retrasos multimillonarios?",
            "What contracting model will you enforce on aerospace suppliers to prevent multi-billion-dollar delays?",
            {
                "A) Contratos de precio fijo por hitos verificables y auditoría abierta",
                "B) Adjudicación directa sin penalizaciones por retrasos de entrega",
                "C) Rediseñar los motores principales después de fabricar los tanques"
            },
            {
                "A) Firm-fixed-price milestone contracts with open-book audits",
                "B) Sole-source awards with zero penalties for schedule slips",
                "C) Redesign main engines after structural tanks are already welded"
            },
            {
                "Sr. Hale celebra la disciplina financiera: solo se paga tras superar pruebas.",
                "Sr. Hale rechaza pagar sobrecostes ilimitados sin responsabilidad.",
                "Cambiar motores tarde destruye la integración estructural y el presupuesto."
            },
            {
                "Mr. Hale applauds financial discipline: payment only upon verified tests.",
                "Mr. Hale refuses to fund unlimited cost-plus overruns with zero accountability.",
                "Late engine redesigns wreck structural integration and budgets."
            },
            {  0.10f, -0.22f, -0.18f },
            {  0.07f, -0.14f, -0.12f },
            {  0.05f, -0.08f, -0.18f }
        },
        // 11 -> C
        {
            "11/12 · DESCENSO EN POLO SUR", "11/12 · SOUTH POLE LANDING",
            "PRESIDENTE BROOKS (Comité Espacial)", "CHAIRMAN BROOKS (Space Committee)",
            "El polo sur lunar tiene cráteres escarpados y sombras largas. ¿Cómo garantizamos un alunizaje seguro?",
            "The lunar South Pole has steep craters and long shadows. How do we ensure a safe touchdown?",
            {
                "A) Descenso cronometrado a ciegas sin radar Doppler de superficie",
                "B) Aterrizar en el ecuador lunar ignorando los depósitos de hielo",
                "C) Navegación óptica por terreno (TRN) con LIDAR hacia cráteres polares"
            },
            {
                "A) Timed blind descent with no surface Doppler radar",
                "B) Land on the lunar equator ignoring polar water-ice deposits",
                "C) Terrain Relative Navigation (TRN) with LIDAR targeting polar rims"
            },
            {
                "Aterrizar sin radar ni evasión de rocas acabaría volcando el módulo.",
                "Abandonar el polo sur renuncia al hielo de agua clave para futuras bases.",
                "El comité valida el sistema LIDAR autónomo para evitar rocas y pendientes."
            },
            {
                "Landing without radar or hazard avoidance risks tipping the lander.",
                "Abandoning the South Pole forfeits water ice vital for sustained bases.",
                "The committee validates autonomous LIDAR hazard avoidance."
            },
            { -0.10f, -0.06f,  0.06f },
            { -0.20f, -0.15f,  0.10f },
            { -0.22f, -0.08f,  0.09f }
        },
        // 12 -> B
        {
            "12/12 · RETORNO CIENTÍFICO Y ROI", "12/12 · SCIENCE RETURN & ROI",
            "PRESIDENTE BROOKS & SR. HALE", "CHAIRMAN BROOKS & MR. HALE",
            "Última cuestión antes de votar: ¿qué retorno tangible justificará esta inversión ante la ciencia y el Senado?",
            "Final question before the vote: what tangible return justifies this investment to scientists and the Senate?",
            {
                "A) No embarcar instrumentos científicos para reducir peso y coste",
                "B) Retorno de muestras criogénicas, sismómetros y retrorreflectores láser",
                "C) Plantar una bandera y despegar en 10 minutos sin recoger regolito"
            },
            {
                "A) Exclude all scientific instruments to save mass and cost",
                "B) Cryogenic sample return, seismometers, and laser retroreflectors",
                "C) Plant a flag and depart in 10 minutes without collecting regolith"
            },
            {
                "Sr. Hale recuerda que sin retorno científico el Senado vetará los fondos.",
                "Impresión final sólida: ciencia de alto valor y liderazgo tecnológico.",
                "El comité considera insuficiente un viaje sin instrumentación científica."
            },
            {
                "Mr. Hale notes that without science return the Senate will veto funding.",
                "Strong closing impression: high-value science and technological leadership.",
                "The committee deems a flight with zero scientific instruments insufficient."
            },
            { -0.12f,  0.09f, -0.10f },
            { -0.18f,  0.10f, -0.16f },
            { -0.14f,  0.08f, -0.12f }
        }
    };
}

GameState CommitteePhase::Update(SharedMissionData& data) {
    Vector2 mouse = GetMousePosition();
    float dt = GetFrameTime();

    if (saveNoticeTimer > 0.0f) saveNoticeTimer = std::max(0.0f, saveNoticeTimer - dt);
    if (deltaFlashTimer > 0.0f) deltaFlashTimer = std::max(0.0f, deltaFlashTimer - dt);

    int totalQ = static_cast<int>(questions.size());
    int delibStep = totalQ + 1;
    int revealStep = totalQ + 2;

    data.difficulty = s_selectedDifficulty;   // la misma dificultad ajusta después los problemas del viaje

    // Voz "gibberish": cada vez que cambia la frase en pantalla habla quien toca
    int key = step * 10 + (step == delibStep ? deliberationLine : 0);
    if (key != voiceKey) {
        bool answered = (voiceKey >= 10 && step > voiceKey / 10);   // la economista acaba de responder
        voiceKey = key;
        VoiceStop();
        if (answered) VoiceSay(lastFeedbackEs, VOICE_HERO, 6);
        if (step >= 1 && step <= totalQ) {
            const auto& q = questions[step - 1];
            float pitch = VOICE_BROOKS;
            if (q.speakerEs.find("HALE") != std::string::npos) pitch = VOICE_HALE;
            else if (q.speakerEs.find("VANCE") != std::string::npos) pitch = VOICE_VANCE;
            VoiceSay(q.textEs, pitch);
        } else if (step == delibStep) {
            const float pitches[3] = { VOICE_HALE, VOICE_VANCE, VOICE_BROOKS };
            VoiceSay("deliberacion del tribunal en sesion cerrada con veredicto", pitches[std::clamp(deliberationLine, 0, 2)]);
        } else if (step == revealStep) {
            SfxTone(523.0f, 0.12f); SfxTone(659.0f, 0.22f, 'q', 0.05f); SfxTone(784.0f, 0.34f, 'q', 0.05f);
        }
    }

    if (step == revealStep) {
        epicRevealTimer += dt;
        float progress = std::clamp(epicRevealTimer / 1.8f, 0.0f, 1.0f);
        float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
        displayedBudget = data.awardedBudget * eased;
    }

    Rectangle btnMenu  = { 24.0f, 14.0f, 105.0f, 32.0f };
    Rectangle btnSave  = { 138.0f, 14.0f, 125.0f, 32.0f };
    Rectangle btnFacts = { 330.0f, 66.0f, 142.0f, 34.0f };

    if (IsKeyPressed(KEY_M)) return GameState::MENU;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mouse, btnMenu)) {
            return GameState::MENU;
        }
        if (CheckCollisionPointRec(mouse, btnSave)) {
            SaveManager::SaveGame(data.activeSaveSlot, data, GameState::COMMITTEE);
            saveNoticeTimer = 2.0f;
            return GameState::COMMITTEE;
        }
        if (CheckCollisionPointRec(mouse, btnFacts)) {
            showNasaFacts = !showNasaFacts;
            return GameState::COMMITTEE;
        }

        if (showNasaFacts) {
            Rectangle btnPrevPage = { 250.0f, 470.0f, 160.0f, 36.0f };
            Rectangle btnNextPage = { 425.0f, 470.0f, 160.0f, 36.0f };
            Rectangle btnClose    = { 880.0f, 470.0f, 150.0f, 36.0f };

            if (CheckCollisionPointRec(mouse, btnPrevPage)) {
                nasaFactsPage = (nasaFactsPage + 2) % 3;
                return GameState::COMMITTEE;
            }
            if (CheckCollisionPointRec(mouse, btnNextPage)) {
                nasaFactsPage = (nasaFactsPage + 1) % 3;
                return GameState::COMMITTEE;
            }
            if (CheckCollisionPointRec(mouse, btnClose)) {
                showNasaFacts = false;
                return GameState::COMMITTEE;
            }
            return GameState::COMMITTEE;
        }

        if (step == 0) {
            // Selector de dificultad (0: Cadete, 1: Ingeniero, 2: Comandante)
            for (int d = 0; d < 3; ++d) {
                Rectangle diffBtn = { 265.0f + d * 322.0f, 532.0f, 306.0f, 54.0f };
                if (CheckCollisionPointRec(mouse, diffBtn)) {
                    s_selectedDifficulty = d;
                    DifficultyConfig cfg = GetDifficultyConfig(s_selectedDifficulty);
                    trustHale = cfg.startTrust;
                    trustBrooks = cfg.startTrust;
                    trustVance = cfg.startTrust;
                    return GameState::COMMITTEE;
                }
            }

            Rectangle btnNext = { 965.0f, 650.0f, 250.0f, 44.0f };
            if (CheckCollisionPointRec(mouse, btnNext)) {
                step = 1;
            }
        } else if (step >= 1 && step <= totalQ) {
            int qIdx = step - 1;
            DifficultyConfig cfg = GetDifficultyConfig(s_selectedDifficulty);

            auto applyScale = [&](float rawDelta) -> float {
                return (rawDelta >= 0.0f) ? (rawDelta * cfg.gainMultiplier) : (rawDelta * cfg.lossMultiplier);
            };

            for (int i = 0; i < 3; ++i) {
                Rectangle optBtn = { 262.0f, 562.0f + i * 44.0f, 955.0f, 38.0f };
                if (CheckCollisionPointRec(mouse, optBtn)) {
                    float prevH = trustHale;
                    float prevB = trustBrooks;
                    float prevV = trustVance;

                    trustHale   = std::clamp(trustHale   + applyScale(questions[qIdx].haleDelta[i]),   0.02f, 1.0f);
                    trustBrooks = std::clamp(trustBrooks + applyScale(questions[qIdx].brooksDelta[i]), 0.02f, 1.0f);
                    trustVance  = std::clamp(trustVance  + applyScale(questions[qIdx].vanceDelta[i]),  0.02f, 1.0f);

                    lastDeltaHale   = static_cast<int>(std::round((trustHale   - prevH) * 100.0f));
                    lastDeltaBrooks = static_cast<int>(std::round((trustBrooks - prevB) * 100.0f));
                    lastDeltaVance  = static_cast<int>(std::round((trustVance  - prevV) * 100.0f));
                    deltaFlashTimer = 4.0f;

                    lastFeedbackEs = questions[qIdx].feedbackEs[i];
                    lastFeedbackEn = questions[qIdx].feedbackEn[i];

                    step++;
                    if (step == delibStep) {
                        deliberationLine = 0;
                        float avg = (trustHale + trustBrooks + trustVance) / 3.0f;
                        data.awardedBudget = cfg.baseBudget + std::pow(avg, 1.15f) * cfg.budgetSpan;
                        data.remainingBudget = data.awardedBudget;
                    }
                    break;
                }
            }
        } else if (step == delibStep) {
            Rectangle btnDelibNext = { 895.0f, 650.0f, 320.0f, 44.0f };
            if (CheckCollisionPointRec(mouse, btnDelibNext)) {
                if (deliberationLine < 2) {
                    deliberationLine++;
                } else {
                    step = revealStep;
                    epicRevealTimer = 0.0f;
                    displayedBudget = 0.0f;
                }
            }
        } else if (step == revealStep) {
            bool goodBudget = (data.awardedBudget >= 1340.0f);
            if (!goodBudget) {
                Rectangle btnRetry = { 620.0f, 650.0f, 275.0f, 44.0f };
                if (CheckCollisionPointRec(mouse, btnRetry)) {
                    Init(data.lang);
                    return GameState::COMMITTEE;
                }
            }
            Rectangle btnToLab = { 910.0f, 650.0f, 305.0f, 44.0f };
            if (CheckCollisionPointRec(mouse, btnToLab)) {
                return GameState::HANGAR;
            }
        }
    }

    return GameState::COMMITTEE;
}

void CommitteePhase::Draw(const SharedMissionData& data) {
    int totalQ = static_cast<int>(questions.size());
    int delibStep = totalQ + 1;
    int revealStep = totalQ + 2;

    if (texBg.id > 0) {
        DrawTexturePro(texBg, { 0, 0, (float)texBg.width, (float)texBg.height },
                       { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT }, { 0, 0 }, 0.0f, WHITE);
    } else {
        ClearBackground(Color{ 245, 236, 200, 255 });
    }

    DrawRectangle(24, 14, 105, 32, Color{ 20, 28, 48, 240 });
    DrawRectangleLines(24, 14, 105, 32, GOLD);
    DrawUIText("MENÚ", 50.0f, 20.0f, 18.0f, WHITE);

    DrawRectangle(138, 14, 125, 32, Color{ 38, 125, 62, 240 });
    DrawRectangleLines(138, 14, 125, 32, GOLD);
    DrawUIText(Tr(data.lang, "GUARDAR", "SAVE"), 162.0f, 20.0f, 18.0f, WHITE);

    if (saveNoticeTimer > 0.0f) {
        DrawUIText(Tr(data.lang, "¡Partida guardada!", "Game saved!"), 278.0f, 19.0f, 19.0f, DARKGREEN);
    }

    DrawRectangle(22, 54, 465, 232, Color{ 12, 18, 34, 248 });
    DrawRectangleLinesEx({ 22.0f, 54.0f, 465.0f, 232.0f }, 3.0f, Color{ 242, 193, 78, 255 });
    DrawUIText(Tr(data.lang, "AUDIENCIA PRESUPUESTO", "BUDGET HEARING"), 36.0f, 70.0f, 20.0f, Color{ 242, 193, 78, 255 });

    DrawRectangle(330, 66, 142, 34, Color{ 242, 193, 78, 255 });
    DrawUIText(Tr(data.lang, "DATOS NASA", "NASA FACTS"), 347.0f, 73.0f, 18.0f, BLACK, false);

    if (step < revealStep) {
        DrawUIText(step == delibStep ? Tr(data.lang, "DELIBERANDO...", "DELIBERATING...")
                                     : Tr(data.lang, "$ ??? MILLONES", "$ ??? MILLION"),
                   36.0f, 106.0f, 32.0f, step == delibStep ? SKYBLUE : WHITE);
    } else {
        bool goodBudget = (data.awardedBudget >= 1340.0f);
        DrawUIText(TextFormat(Tr(data.lang, "$ %.0f MILLONES", "$ %.0f MILLION"), displayedBudget),
                   36.0f, 106.0f, 34.0f, goodBudget ? GREEN : Color{ 255, 85, 85, 255 });
    }

    auto drawMemberRow = [&](const char* name, float val, int delta, float y) {
        Color barCol = GetApprovalColor(val);
        DrawUIText(name, 36.0f, y, 18.0f, LIGHTGRAY);

        DrawRectangle(195, static_cast<int>(y + 4), 140, 14, Color{ 25, 34, 58, 255 });
        DrawRectangle(195, static_cast<int>(y + 4), static_cast<int>(140.0f * val), 14, barCol);
        DrawRectangleLines(195, static_cast<int>(y + 4), 140, 14, Color{ 90, 105, 135, 255 });

        DrawUIText(TextFormat("%d%%", (int)std::round(val * 100.0f)), 344.0f, y, 18.0f, barCol);

        if (deltaFlashTimer > 0.0f && delta != 0) {
            Color dCol = (delta > 0) ? GREEN : RED;
            DrawUIText(TextFormat("%+d%%", delta), 408.0f, y, 18.0f, dCol);
        }
    };

    drawMemberRow("SR. HALE", trustHale, lastDeltaHale, 158.0f);
    drawMemberRow(Tr(data.lang, "PDTE. BROOKS", "CHMN. BROOKS"), trustBrooks, lastDeltaBrooks, 194.0f);
    drawMemberRow("DRA. VANCE", trustVance, lastDeltaVance, 230.0f);

    Color colHale   = (step == delibStep && deliberationLine == 0) ? WHITE : Color{ 235, 235, 235, 255 };
    Color colBrooks = (step == delibStep && deliberationLine == 2) ? WHITE : Color{ 235, 235, 235, 255 };
    Color colVance  = (step == delibStep && deliberationLine == 1) ? WHITE : Color{ 235, 235, 235, 255 };

    // Los personajes se dibujan con la proporción real de su imagen (antes salían ensanchados)
    auto drawPerson = [](Texture2D t, Rectangle box, Color tint) {
        if (t.id <= 0) return;
        float w = box.height * static_cast<float>(t.width) / static_cast<float>(t.height);
        DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                       { box.x + (box.width - w) * 0.5f, box.y, w, box.height }, { 0, 0 }, 0.0f, tint);
    };
    drawPerson(texGrey,  { 625, 80, 150, 260 }, colHale);
    drawPerson(texBlue,  { 830, 80, 150, 260 }, colBrooks);
    drawPerson(texSlick, { 1030, 80, 150, 260 }, colVance);

    auto drawTrustBar = [](int x, float val) {
        DrawRectangle(x, 100, 24, 145, Color{ 15, 20, 35, 255 });
        int fillH = static_cast<int>(141.0f * std::clamp(val, 0.0f, 1.0f));
        Color gradCol = GetApprovalColor(val);
        DrawRectangle(x + 2, 100 + (143 - fillH), 20, fillH, gradCol);
        DrawRectangleLines(x, 100, 24, 145, BLACK);
    };
    drawTrustBar(770, trustHale);
    drawTrustBar(972, trustBrooks);
    drawTrustBar(1172, trustVance);

    if (texDesk.id > 0) {
        DrawTexturePro(texDesk, { 0, 0, (float)texDesk.width, (float)texDesk.height },
                       { 470, 240, 810, 360 }, { 0, 0 }, 0.0f, WHITE);
    }

    drawPerson(texHero, { 15, 425, 225, 295 }, WHITE);

    if (step >= 2 && step <= delibStep && !lastFeedbackEs.empty()) {
        DrawRectangle(498, 374, 742, 56, Color{ 14, 21, 38, 248 });
        DrawRectangleLinesEx({ 498.0f, 374.0f, 742.0f, 56.0f }, 2.0f, SKYBLUE);

        DrawUIText(Tr(data.lang, lastFeedbackEs.c_str(), lastFeedbackEn.c_str()), 512.0f, 382.0f, 16.0f, WHITE);

        int avgDelta = static_cast<int>(std::round((lastDeltaHale + lastDeltaBrooks + lastDeltaVance) / 3.0f));
        Color dCol = (avgDelta >= 0) ? GREEN : RED;
        DrawUIText(TextFormat(Tr(data.lang,
                   "Variación Aprobación:  Hale %+d%%  |  Brooks %+d%%  |  Vance %+d%%",
                   "Approval Change:  Hale %+d%%  |  Brooks %+d%%  |  Vance %+d%%"),
                   lastDeltaHale, lastDeltaBrooks, lastDeltaVance),
                   512.0f, 405.0f, 15.0f, dCol);
    }

    DrawRectangle(245, 488, 995, 216, Color{ 247, 241, 225, 250 });
    DrawRectangleLinesEx({ 245.0f, 488.0f, 995.0f, 216.0f }, 3.0f, Color{ 15, 20, 35, 255 });

    if (step == 0) {
        DrawRectangle(265, 468, 265, 30, Color{ 15, 20, 35, 255 });
        DrawUIText(Tr(data.lang, "PREPARACIÓN DE MISIÓN", "MISSION BRIEFING"), 282.0f, 473.0f, 17.0f, GOLD);

        DrawUIText(Tr(data.lang,
            "SELECCIONA LA DIFICULTAD DE LA AUDITORÍA DEL CONGRESO (12 RONDAS):",
            "SELECT CONGRESSIONAL HEARING DIFFICULTY (12 ROUNDS):"),
            265.0f, 504.0f, 20.0f, BLACK, false);

        const char* diffTitlesEs[3] = { "1. CADETE (FÁCIL)", "2. INGENIERO (NORMAL)", "3. COMANDANTE (DIFÍCIL)" };
        const char* diffTitlesEn[3] = { "1. CADET (EASY)", "2. ENGINEER (NORMAL)", "3. COMMANDER (HARD)" };
        const char* diffDescEs[3]   = { "Inicio 54% · Fallos -50%", "Inicio 38% · Estándar", "Inicio 24% · Crítico +40%" };
        const char* diffDescEn[3]   = { "Start 54% · Loss -50%", "Start 38% · Standard", "Start 24% · Critical +40%" };
        Color diffAccent[3]         = { GREEN, GOLD, Color{ 255, 95, 95, 255 } };

        for (int d = 0; d < 3; ++d) {
            Rectangle diffBtn = { 265.0f + d * 322.0f, 532.0f, 306.0f, 54.0f };
            bool selected = (s_selectedDifficulty == d);
            bool hover = CheckCollisionPointRec(GetMousePosition(), diffBtn);

            DrawRectangleRec(diffBtn, selected ? Color{ 24, 46, 86, 255 } : (hover ? Color{ 34, 44, 70, 255 } : Color{ 16, 22, 38, 255 }));
            DrawRectangleLinesEx(diffBtn, selected ? 2.5f : 1.5f, selected ? diffAccent[d] : GRAY);

            DrawUIText(Tr(data.lang, diffTitlesEs[d], diffTitlesEn[d]), diffBtn.x + 12.0f, diffBtn.y + 7.0f, 17.0f, selected ? diffAccent[d] : WHITE);
            DrawUIText(Tr(data.lang, diffDescEs[d], diffDescEn[d]), diffBtn.x + 12.0f, diffBtn.y + 30.0f, 14.0f, LIGHTGRAY);
        }

        // Cita historica de astronauta segun dificultad seleccionada
        const char* quoteEs = "\"Las estrellas no parecen más grandes, pero se ven muchísimo más brillantes.\" — Sally Ride (STS-7)";
        const char* quoteEn = "\"The stars don't look bigger, but they look much brighter.\" — Sally Ride (STS-7)";

        if (s_selectedDifficulty == 1) {
            quoteEs = "\"El misterio crea asombro, y el asombro es la base del deseo humano por comprender.\" — Neil Armstrong (Apolo 11)";
            quoteEn = "\"Mystery creates wonder and wonder is the basis of man's desire to understand.\" — Neil Armstrong (Apollo 11)";
        } else if (s_selectedDifficulty == 2) {
            quoteEs = "\"Existen personas que hacen que las cosas pasen, personas que miran lo que pasa y personas que se preguntan qué pasó.\" — Jim Lovell (Apolo 13)";
            quoteEn = "\"There are people who make things happen, people who watch things happen, and people who wonder what happened.\" — Jim Lovell (Apollo 13)";
        }

        DrawRectangle(265, 593, 950, 50, Color{ 16, 24, 44, 250 });
        DrawRectangle(265, 593, 6, 50, diffAccent[s_selectedDifficulty]);
        DrawUITextBoxed(Tr(data.lang, quoteEs, quoteEn),
                        { 282.0f, 598.0f, 920.0f, 42.0f }, 15.0f, 2.0f, Color{ 245, 232, 185, 255 });

        DrawUIText(Tr(data.lang,
            "Consejo: Revisa DATOS NASA arriba a la izquierda antes de responder.",
            "Tip: Check NASA FACTS at top-left before answering."),
            265.0f, 661.0f, 16.0f, DARKGRAY, false);

        Rectangle btnNext = { 965.0f, 650.0f, 250.0f, 44.0f };
        DrawRectangleRec(btnNext, Color{ 15, 20, 35, 255 });
        DrawRectangleLinesEx(btnNext, 2.0f, GOLD);
        DrawUIText(Tr(data.lang, "COMENZAR >", "START HEARING >"), 1022.0f, 660.0f, 20.0f, GOLD);
    } else if (step >= 1 && step <= totalQ) {
        const auto& q = questions[step - 1];
        DrawRectangle(265, 468, 410, 30, Color{ 15, 20, 35, 255 });
        DrawUIText(Tr(data.lang, q.speakerEs.c_str(), q.speakerEn.c_str()), 280.0f, 473.0f, 18.0f, GOLD);

        DrawRectangle(985, 468, 235, 30, Color{ 32, 48, 82, 255 });
        DrawUIText(Tr(data.lang, q.topicEs.c_str(), q.topicEn.c_str()), 998.0f, 473.0f, 16.0f, SKYBLUE);

        DrawUITextBoxed(Tr(data.lang, q.textEs.c_str(), q.textEn.c_str()),
                        { 265.0f, 504.0f, 950.0f, 54.0f }, 19.0f, 2.0f, BLACK, false);

        for (int i = 0; i < 3; ++i) {
            Rectangle optBtn = { 262.0f, 562.0f + i * 44.0f, 955.0f, 38.0f };
            bool hover = CheckCollisionPointRec(GetMousePosition(), optBtn);
            DrawRectangleRec(optBtn, hover ? Color{ 34, 54, 94, 255 } : Color{ 18, 25, 44, 255 });
            DrawRectangleLinesEx(optBtn, 1.5f, hover ? WHITE : GOLD);
            DrawUIText(Tr(data.lang, q.optionsEs[i].c_str(), q.optionsEn[i].c_str()), 278.0f, 570.0f + i * 44.0f, 19.0f, WHITE);
        }
    } else if (step == delibStep) {
        DrawRectangle(265, 468, 430, 30, Color{ 15, 20, 35, 255 });
        DrawUIText(Tr(data.lang, "DELIBERACIÓN DEL TRIBUNAL (SESIÓN CERRADA)", "COMMITTEE DELIBERATION (EXECUTIVE SESSION)"), 280.0f, 473.0f, 17.0f, GOLD);

        const char* judgeSpeaker = "";
        const char* judgeQuote = "";

        if (deliberationLine == 0) {
            judgeSpeaker = Tr(data.lang, "SR. HALE (Tesorero):", "MR. HALE (Treasurer):");
            if (trustHale >= 0.70f) {
                judgeQuote = Tr(data.lang,
                    "\"Los números cuadran al centavo. Su rigor en los hitos de auditoría nos permite liberar una partida presupuestaria sólida sin temor a desvíos fiscales.\"",
                    "\"The numbers check out to the cent. Your rigor on audit milestones allows us to release a strong budget allocation without fiscal overrun fears.\"");
            } else {
                judgeQuote = Tr(data.lang,
                    "\"Existen serias dudas en el control de costes y reservas fiscales. Aplicaremos un recorte severo a los fondos de hardware hasta ver resultados.\"",
                    "\"Serious concerns remain regarding cost controls and fiscal reserves. We must apply a strict cap on hardware funds until results are proven.\"");
            }
        } else if (deliberationLine == 1) {
            judgeSpeaker = Tr(data.lang, "DRA. VANCE (Ingeniera Jefe):", "DR. VANCE (Chief Engineer):");
            if (trustVance >= 0.70f) {
                judgeQuote = Tr(data.lang,
                    "\"Domina la ecuación del cohete, los márgenes de empuje TWR y la reentrada a 11 km/s. Confío plenamente en su criterio para ensamblar la nave.\"",
                    "\"You grasp the rocket equation, TWR liftoff margins, and 11 km/s reentry physics. I fully trust your engineering judgment to assemble the vehicle.\"");
            } else {
                judgeQuote = Tr(data.lang,
                    "\"Cometió errores técnicos graves. Con un presupuesto tan ajustado tendrá que calcular cada tonelada en el hangar o no llegará ni a órbita.\"",
                    "\"You made critical technical errors. With such a tight budget you must calculate every ton in the hangar or you won't even reach orbit.\"");
            }
        } else {
            judgeSpeaker = Tr(data.lang, "PRESIDENTE BROOKS (Comité Espacial):", "CHAIRMAN BROOKS (Space Committee):");
            float avg = (trustHale + trustBrooks + trustVance) / 3.0f;
            if (avg >= 0.70f) {
                judgeQuote = Tr(data.lang,
                    "\"El veredicto es favorable: la misión cuenta con el respaldo total de este Comité. Sr. Hale, proceda a anunciar la dotación oficial aprobada.\"",
                    "\"The verdict is favorable: this mission has the full backing of this Committee. Mr. Hale, please announce the official approved endowment.\"");
            } else {
                judgeQuote = Tr(data.lang,
                    "\"Aprobación mínima por mayoría ajustada. El presupuesto de hardware será muy limitado; cada millón contará en el laboratorio.\"",
                    "\"Narrow approval by split vote. Your hardware budget will be heavily constrained; every single million will count in the assembly lab.\"");
            }
        }

        DrawUIText(judgeSpeaker, 268.0f, 512.0f, 21.0f, Color{ 20, 45, 95, 255 }, false);
        DrawUITextBoxed(judgeQuote, { 268.0f, 545.0f, 945.0f, 95.0f }, 20.0f, 5.0f, BLACK, false);

        Rectangle btnDelibNext = { 895.0f, 650.0f, 320.0f, 44.0f };
        DrawRectangleRec(btnDelibNext, deliberationLine < 2 ? Color{ 32, 54, 96, 255 } : Color{ 185, 130, 20, 255 });
        DrawRectangleLinesEx(btnDelibNext, 2.0f, BLACK);
        DrawUIText(deliberationLine < 2
                   ? Tr(data.lang, "ESCUCHAR SIGUIENTE JUEZ >", "HEAR NEXT JUDGE >")
                   : Tr(data.lang, "REVELAR PRESUPUESTO >", "REVEAL BUDGET >"),
                   920.0f, 660.0f, 19.0f, WHITE);
    } else {
        bool goodBudget = (data.awardedBudget >= 1340.0f);
        float avgPct = ((trustHale + trustBrooks + trustVance) / 3.0f) * 100.0f;
        float programBillions = (displayedBudget * 35.0f) / 1000.0f;

        if (goodBudget) {
            float pulse = 1.0f + 0.04f * std::sin(epicRevealTimer * 6.0f);

            DrawRectangle(252, 495, 981, 202, Color{ 255, 232, 165, 255 });
            DrawRectangleLinesEx({ 252.0f, 495.0f, 981.0f, 202.0f }, 3.0f, Color{ 212, 155, 25, 255 });

            DrawRectangle(265, 468, 390, 30, Color{ 32, 125, 58, 255 });
            DrawUIText(Tr(data.lang, "RESOLUCIÓN APROBADA POR EL CONGRESO", "RESOLUTION APPROVED BY CONGRESS"), 280.0f, 473.0f, 17.0f, WHITE);

            DrawUIText(TextFormat(Tr(data.lang,
                       "APROBACIÓN MEDIA DEL COMITÉ: %.0f%%   (Programa plurianual: $%.1fB)",
                       "AVERAGE COMMITTEE APPROVAL: %.0f%%   (Multi-year program: $%.1fB)"), avgPct, programBillions),
                       272.0f, 508.0f, 19.0f, DARKGRAY, false);

            DrawUIText(TextFormat(Tr(data.lang,
                       "PRESUPUESTO HANGAR: $ %.0f MILLONES",
                       "HANGAR HARDWARE BUDGET: $ %.0f MILLION"), displayedBudget),
                       272.0f, 536.0f, 34.0f * pulse, Color{ 22, 125, 48, 255 });

            DrawUIText(Tr(data.lang,
                       "¡Enhorabuena! Cuentas con fondos suficientes para ensamblar un cohete lunar.",
                       "Congratulations! You have secured enough funding to build a Moon rocket."),
                       272.0f, 586.0f, 20.0f, BLACK, false);

            DrawUIText(Tr(data.lang,
                       "Se auto-guardará al entrar en el Hangar de cohetes.",
                       "Progress will auto-save upon entering the Rocket Hangar."),
                       272.0f, 620.0f, 18.0f, DARKGRAY, false);

            Rectangle btnToLab = { 910.0f, 650.0f, 305.0f, 44.0f };
            DrawRectangleRec(btnToLab, Color{ 38, 125, 62, 255 });
            DrawRectangleLinesEx(btnToLab, 2.0f, BLACK);
            DrawUIText(Tr(data.lang, "IR AL LABORATORIO >", "GO TO ASSEMBLY LAB >"), 942.0f, 660.0f, 20.0f, WHITE);
        } else {
            DrawRectangle(252, 495, 981, 202, Color{ 56, 20, 24, 252 });
            DrawRectangleLinesEx({ 252.0f, 495.0f, 981.0f, 202.0f }, 3.0f, Color{ 225, 55, 55, 255 });

            DrawRectangle(265, 468, 435, 30, Color{ 175, 32, 32, 255 });
            DrawUIText(Tr(data.lang, "AUDIENCIA SUSPENSA · RECORTE DE EMERGENCIA", "HEARING FAILED · EMERGENCY BUDGET CUT"), 280.0f, 473.0f, 17.0f, WHITE);

            DrawUIText(TextFormat(Tr(data.lang,
                       "APROBACIÓN MEDIA: %.0f%% — EL COMITÉ RECHAZA FINANCIAR EL COHETE LUNAR",
                       "AVERAGE APPROVAL: %.0f%% — COMMITTEE REFUSES TO FUND LUNAR ROCKET"), avgPct),
                       272.0f, 508.0f, 19.0f, Color{ 255, 165, 165, 255 });

            DrawUIText(TextFormat(Tr(data.lang,
                       "PRESUPUESTO RECORTADO: $ %.0f MILLONES (INSUFICIENTE)",
                       "SLASHED BUDGET: $ %.0f MILLION (INSUFFICIENT)"), displayedBudget),
                       272.0f, 538.0f, 30.0f, Color{ 255, 80, 80, 255 });

            DrawUIText(Tr(data.lang,
                       "Con menos de $1.340M no podrás costear tanques ni motores para alcanzar la Luna.",
                       "With less than $1,340M you cannot afford tanks or engines to reach the Moon."),
                       272.0f, 584.0f, 19.0f, WHITE);

            DrawUIText(Tr(data.lang,
                       "Puedes repetir la audiencia para mejorar tu nota o entrar al Hangar con este recorte.",
                       "You may retry the hearing to improve your score or enter the Hangar with this cut."),
                       272.0f, 614.0f, 17.0f, LIGHTGRAY);

            Rectangle btnRetry = { 620.0f, 650.0f, 275.0f, 44.0f };
            DrawRectangleRec(btnRetry, Color{ 165, 42, 42, 255 });
            DrawRectangleLinesEx(btnRetry, 2.0f, WHITE);
            DrawUIText(Tr(data.lang, "REINTENTAR AUDIENCIA", "RETRY HEARING"), 650.0f, 661.0f, 18.0f, WHITE);

            Rectangle btnToLab = { 910.0f, 650.0f, 305.0f, 44.0f };
            DrawRectangleRec(btnToLab, Color{ 75, 82, 96, 255 });
            DrawRectangleLinesEx(btnToLab, 2.0f, LIGHTGRAY);
            DrawUIText(Tr(data.lang, "IR AL HANGAR IGUAL >", "GO TO HANGAR ANYWAY >"), 930.0f, 661.0f, 18.0f, WHITE);
        }
    }

    if (showNasaFacts) {
        DrawRectangle(210, 115, 860, 410, Color{ 10, 15, 30, 252 });
        DrawRectangleLinesEx({ 210.0f, 115.0f, 860.0f, 410.0f }, 3.0f, GOLD);

        DrawUIText(TextFormat(Tr(data.lang,
            "ARCHIVO DE AUDITORÍA NASA · PÁGINA %d / 3",
            "NASA AUDIT ARCHIVE · PAGE %d / 3"), nasaFactsPage + 1),
            245.0f, 136.0f, 24.0f, GOLD);

        if (nasaFactsPage == 0) {
            DrawUITextBoxed(Tr(data.lang,
                "1. PRESUPUESTO DE PROGRAMA: El desarrollo Artemis/Apolo requiere ~$93B auditados por hitos con 15% de reserva de contingencia.\n"
                "2. PERFIL LOR: El Encuentro en Órbita Lunar separa la cápsula del módulo lunar, ahorrando decenas de toneladas en rampa.\n"
                "3. EMPUJE-PESO (TWR): Un cohete lunar exige TWR > 1,20 (Saturn V = 1,22) para vencer pérdidas gravitatorias.\n"
                "4. FRACCIÓN DE PROPELENTE: Por Tsiolkovsky, entre el 85% y el 90% de la masa en rampa debe ser propelente.",
                "1. PROGRAM BUDGET: Artemis/Apollo development requires ~$93B milestone-audited with a 15% contingency reserve.\n"
                "2. LOR PROFILE: Lunar Orbit Rendezvous separates the capsule from the lander, saving dozens of tons on the pad.\n"
                "3. THRUST-TO-WEIGHT (TWR): A Moon rocket demands liftoff TWR > 1.20 (Saturn V = 1.22) to beat gravity drag.\n"
                "4. PROPELLANT FRACTION: By Tsiolkovsky's law, 85% to 90% of total pad mass must be propellant."),
                { 245.0f, 182.0f, 790.0f, 275.0f }, 19.0f, 6.0f, WHITE);
        } else if (nasaFactsPage == 1) {
            DrawUITextBoxed(Tr(data.lang,
                "5. CRIOGENIA: RP-1/LOX aporta alto empuje en 1ª etapa, mientras LH2/LOX maximiza el Isp (~450s) en etapas superiores.\n"
                "6. SEGURIDAD (LES): La Torre de Escape sólida extrae la cápsula en milisegundos ante anomalías en el lanzamiento.\n"
                "7. INYECCIÓN TLI: La trayectoria de regreso libre apunta 3 días por delante de la órbita lunar para interceptarla.\n"
                "8. REENTRADA LUNAR: Regresar a ~11 km/s (4x energía de LEO) exige escudo térmico ablativo fenólico y doble salto.",
                "5. CRYOGENICS: RP-1/LOX delivers high liftoff thrust on Stage 1, while LH2/LOX maximizes Isp (~450s) upstairs.\n"
                "6. CREW SAFETY (LES): A solid Launch Escape System pulls the capsule clear in milliseconds during launch anomalies.\n"
                "7. TLI TRAJECTORY: A free-return trajectory aims 3 days ahead of the Moon's orbital path for accurate intercept.\n"
                "8. LUNAR REENTRY: Returning at ~11 km/s (4x LEO kinetic energy) requires a phenolic ablative heat shield."),
                { 245.0f, 182.0f, 790.0f, 275.0f }, 19.0f, 6.0f, WHITE);
        } else {
            DrawUITextBoxed(Tr(data.lang,
                "9. RADIACIÓN ESPACIO PROFUNDO: Los tanques de agua potable rodean el refugio solar junto a ordenadores triples (TMR).\n"
                "10. CONTRATOS: Contratos de precio fijo por hitos verificables y auditoría abierta evitan sobrecostes.\n"
                "11. ALUNIZAJE POLAR: La Navegación Óptica por Terreno (TRN) con LIDAR evita cráteres y sombras en el polo sur.\n"
                "12. RETORNO CIENTÍFICO: Muestras criogénicas, sismómetros y retrorreflectores láser justifican la inversión.",
                "9. DEEP SPACE RADIATION: Potable water tanks buffer the solar storm shelter alongside triple-redundant computers.\n"
                "10. CONTRACTING: Firm-fixed-price milestone contracts with open audits prevent multi-billion-dollar overruns.\n"
                "11. POLAR LANDING: Terrain Relative Navigation (TRN) with LIDAR avoids craters and deep shadows at the South Pole.\n"
                "12. SCIENCE ROI: Cryogenic samples, seismometers, and laser retroreflectors justify Senate funding."),
                { 245.0f, 182.0f, 790.0f, 275.0f }, 19.0f, 6.0f, WHITE);
        }

        DrawRectangle(250, 470, 160, 36, Color{ 35, 55, 95, 255 });
        DrawRectangleLines(250, 470, 160, 36, SKYBLUE);
        DrawUIText(Tr(data.lang, "< ANTERIOR", "< PREV"), 280.0f, 478.0f, 18.0f, WHITE);

        DrawRectangle(425, 470, 160, 36, Color{ 35, 55, 95, 255 });
        DrawRectangleLines(425, 470, 160, 36, SKYBLUE);
        DrawUIText(Tr(data.lang, "SIGUIENTE >", "NEXT >"), 455.0f, 478.0f, 18.0f, WHITE);

        DrawRectangle(880, 470, 150, 36, Color{ 130, 40, 40, 255 });
        DrawRectangleLines(880, 470, 150, 36, WHITE);
        DrawUIText(Tr(data.lang, "CERRAR", "CLOSE"), 920.0f, 478.0f, 18.0f, WHITE);
    }
}

void CommitteePhase::Unload() {
    if (texturesLoaded) {
        if (texBg.id > 0) UnloadTexture(texBg);
        if (texDesk.id > 0) UnloadTexture(texDesk);
        if (texHero.id > 0) UnloadTexture(texHero);
        if (texGrey.id > 0) UnloadTexture(texGrey);
        if (texBlue.id > 0) UnloadTexture(texBlue);
        if (texSlick.id > 0) UnloadTexture(texSlick);
        texturesLoaded = false;
    }
}