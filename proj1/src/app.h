#ifndef APP_H
#define APP_H

//--------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------
#include "../../shared/sync.h"
#include "raylib.h"

//#define RAYMAP_IMPLEMENTATION
#include "../../lib/raymap.h"
#include "../../lib/raymapvid.h"

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

#define APP_SCREEN_WIDTH    1920
#define APP_SCREEN_HEIGHT   1080
#define APP_TARGET_FPS      60

#define APP_CONFIG          "config/surface.cfg"

#define APP_VIDEO_PRERECORDED   "assets/video/prerecorded.webm"
#define APP_VIDEO_RT_BG         "assets/video/rt_bg.webm"
#define APP_VIDEO_RT_FG         "assets/video/output_new.webm"

//--------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------

// Mode de contenu — global, s'applique à toute l'installation
typedef enum {
    MODE_PRERECORDED = 0,   // Vidéo pré-enregistrée en boucle
    MODE_REALTIME           // 2 vidéos superposées, opacité pilotée
} AppMode;

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------

// État de la surface
typedef struct {
    RM_Surface    *surface;         // Surface de mapping raymap
    RM_Calibration calibration;     // État de calibration

    // Vidéos
    RMV_Video     *videoPrerecorded; // Vidéo pré-enregistrée
    RMV_Video     *videoBg;          // Temps réel — fond
    RMV_Video     *videoFg;          // Temps réel — avant

    // Paramètres temps réel
    float          fgOpacity;        // Opacité vidéo avant [0.0 - 1.0]
    float          fgSpeed;          // Vitesse video avant
} SurfaceState;

// État global de l'application
typedef struct {
    SurfaceState  surf;             // La surface unique de proj1
    AppMode       mode;             // Mode actif global
    bool          hudVisible;       // Overlay régie visible
    bool          running;          // Boucle principale active
    float         espValue;
} AppState;

//--------------------------------------------------------------------------------------
// Fonctions — app.c
//--------------------------------------------------------------------------------------

// Initialise fenêtre, surface, vidéos, config
bool AppInit(AppState *app);

// Libère toutes les ressources
void AppShutdown(AppState *app);

// Gère tous les inputs clavier (switch mode, calibration, HUD)
void AppHandleInput(AppState *app);

// Met à jour l'état (vidéos, opacité)
void AppUpdate(AppState *app, float dt);

// Dessine le frame complet
void AppDraw(AppState *app, float dt);

//--------------------------------------------------------------------------------------
// Fonctions — render.c
//--------------------------------------------------------------------------------------

// Contenu temps réel dessiné sur la surface — à adapter selon l'installation
void DrawRealtime(AppState *app, float dt);

//--------------------------------------------------------------------------------------
// Fonctions — hud.c
//--------------------------------------------------------------------------------------

// Overlay régie
void DrawHUD(const AppState *app);

#endif // APP_H
