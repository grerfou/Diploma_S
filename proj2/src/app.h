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

#define APP_CONFIG_A            "config/surface_a.cfg"
#define APP_CONFIG_B            "config/surface_b.cfg"

#define APP_VIDEO_PRERECORDED   "assets/video/prerecorded.webm"
#define APP_VIDEO_RT_BG_A       "assets/video/rt_bg_a.mp4"
#define APP_VIDEO_RT_FG_A       "assets/video/rt_fg_a.mp4"
#define APP_VIDEO_RT_BG_B       "assets/video/rt_bg_b.mp4"
#define APP_VIDEO_RT_FG_B       "assets/video/rt_fg_b.mp4"

//--------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------

// Mode de contenu — global, s'applique à toute l'installation
typedef enum {
    MODE_PRERECORDED = 0,   // Vidéo pré-enregistrée en boucle
    MODE_REALTIME           // 2 vidéos superposées, opacité pilotée
} AppMode;

// Surface active en régie
typedef enum {
    SURFACE_A = 0,
    SURFACE_B = 1
} ActiveSurface;

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------

// État d'une surface individuelle
typedef struct {
    RM_Surface    *surface;          // Surface de mapping raymap
    RM_Calibration calibration;      // État de calibration

    // Vidéos
    RMV_Video     *videoPrerecorded; // Vidéo pré-enregistrée (partagée)
    RMV_Video     *videoBg;          // Temps réel — fond
    RMV_Video     *videoFg;          // Temps réel — avant

    // Paramètres temps réel
    float          fgOpacity;        // Opacité vidéo avant [0.0 - 1.0]
    float          fgSpeed;          // Vitesse vidéo avant
} SurfaceState;

// État global de l'application
typedef struct {
    SurfaceState  surfaces[2];      // Les 2 surfaces
    AppMode       mode;             // Mode actif global
    ActiveSurface activeSurface;    // Surface sélectionnée en régie
    bool          hudVisible;       // Overlay régie visible
    bool          running;          // Boucle principale active
    float         espValue;
} AppState;

//--------------------------------------------------------------------------------------
// Fonctions — app.c
//--------------------------------------------------------------------------------------

bool AppInit(AppState *app);
void AppShutdown(AppState *app);
void AppHandleInput(AppState *app);
void AppUpdate(AppState *app, float dt);
void AppDraw(AppState *app, float dt);

//--------------------------------------------------------------------------------------
// Fonctions — render.c
//--------------------------------------------------------------------------------------

void DrawRealtime(AppState *app, int surfaceIndex, float dt);

//--------------------------------------------------------------------------------------
// Fonctions — hud.c
//--------------------------------------------------------------------------------------

void DrawHUD(const AppState *app);

#endif // APP_H
