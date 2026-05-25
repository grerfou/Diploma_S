/*******************************************************************************
*
*   proj1 — app.c
*   Initialisation, gestion des inputs, update et draw de l'application.
*
*******************************************************************************/
#include "app.h"
#include <stdio.h>

//------------------------------------------------------------------------------
// AppInit
//------------------------------------------------------------------------------

bool AppInit(AppState *app)
{
    if (!app) return false;

    // Fenêtre
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    InitWindow(APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, "proj1");
    SetWindowPosition(0, 0);
    SetTargetFPS(APP_TARGET_FPS);

    // État initial
    app->mode       = MODE_PRERECORDED;
    app->hudVisible = true;
    app->running    = true;

    // --- Surface ---
    app->surf.surface = RM_CreateSurface(APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, RM_MAP_HOMOGRAPHY);
    if (!app->surf.surface) {
        TraceLog(LOG_ERROR, "APP: Failed to create surface");
        return false;
    }

    // Charge la calibration ou recentre par défaut
    if (!RM_LoadConfig(app->surf.surface, APP_CONFIG)) {
        TraceLog(LOG_WARNING, "APP: No config found, using default quad");
        RM_ResetQuad(app->surf.surface, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    }

    // Calibration
    app->surf.calibration = RM_CalibrationDefault(app->surf.surface);
    app->surf.calibration.enabled = false;

    // Opacité fg par défaut
    app->surf.fgOpacity = 1.0f;

    // --- Vidéos ---
    app->surf.videoPrerecorded = RMV_LoadVideo(APP_VIDEO_PRERECORDED);
    if (!app->surf.videoPrerecorded) {
        TraceLog(LOG_ERROR, "APP: Failed to load prerecorded video: %s", APP_VIDEO_PRERECORDED);
        return false;
    }

    app->surf.videoBg = RMV_LoadVideo(APP_VIDEO_RT_BG);
    if (!app->surf.videoBg) {
        TraceLog(LOG_ERROR, "APP: Failed to load RT background video: %s", APP_VIDEO_RT_BG);
        return false;
    }

    app->surf.videoFg = RMV_LoadVideo(APP_VIDEO_RT_FG);
    if (!app->surf.videoFg) {
        TraceLog(LOG_ERROR, "APP: Failed to load RT foreground video: %s", APP_VIDEO_RT_FG);
        return false;
    }

    // Toutes les vidéos en boucle
    RMV_SetVideoLoop(app->surf.videoPrerecorded, true);
    RMV_SetVideoLoop(app->surf.videoBg, true);
    RMV_SetVideoLoop(app->surf.videoFg, true);

    // Démarre la vidéo pré-enregistrée (mode par défaut)
    RMV_PlayVideo(app->surf.videoPrerecorded);

    TraceLog(LOG_INFO, "APP: Initialized successfully");
    return true;
}

//------------------------------------------------------------------------------
// AppShutdown
//------------------------------------------------------------------------------

void AppShutdown(AppState *app)
{
    if (!app) return;

    // Sauvegarde la calibration
    RM_SaveConfig(app->surf.surface, APP_CONFIG);

    // Vidéos
    if (app->surf.videoPrerecorded) RMV_UnloadVideo(app->surf.videoPrerecorded);
    if (app->surf.videoBg)          RMV_UnloadVideo(app->surf.videoBg);
    if (app->surf.videoFg)          RMV_UnloadVideo(app->surf.videoFg);

    // Surface
    if (app->surf.surface) RM_DestroySurface(app->surf.surface);

    CloseWindow();

    TraceLog(LOG_INFO, "APP: Shutdown complete");
}

//------------------------------------------------------------------------------
// AppHandleInput
//------------------------------------------------------------------------------

void AppHandleInput(AppState *app)
{
    if (!app) return;

    // Quitter
    if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE)) {
        app->running = false;
        return;
    }

    // Toggle HUD
    if (IsKeyPressed(KEY_H)) {
        app->hudVisible = !app->hudVisible;
    }

    // Switch mode pré-enregistré ↔ temps réel
    if (IsKeyPressed(KEY_TAB)) {
        if (app->mode == MODE_PRERECORDED) {
            // Passer en temps réel
            RMV_PauseVideo(app->surf.videoPrerecorded);
            RMV_PlayVideo(app->surf.videoBg);
            RMV_PlayVideo(app->surf.videoFg);
            app->mode = MODE_REALTIME;
            TraceLog(LOG_INFO, "APP: Switched to REALTIME mode");
        } else {
            // Repasser en pré-enregistré
            RMV_StopVideo(app->surf.videoBg);
            RMV_StopVideo(app->surf.videoFg);
            RMV_PlayVideo(app->surf.videoPrerecorded);
            app->mode = MODE_PRERECORDED;
            TraceLog(LOG_INFO, "APP: Switched to PRERECORDED mode");
        }
    }

    // Calibration — toggle
    if (IsKeyPressed(KEY_C)) {
        RM_ToggleCalibration(&app->surf.calibration);
    }

    // Calibration — reset quad
    if (IsKeyPressed(KEY_R)) {
        RM_ResetCalibrationQuad(&app->surf.calibration, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    }

    // Calibration — save
    if (IsKeyPressed(KEY_S)) {
        if (RM_SaveConfig(app->surf.surface, APP_CONFIG)) {
            TraceLog(LOG_INFO, "APP: Config saved");
        }
    }

    // Calibration — load
    if (IsKeyPressed(KEY_L)) {
        if (RM_LoadConfig(app->surf.surface, APP_CONFIG)) {
            TraceLog(LOG_INFO, "APP: Config loaded");
        }
    }

    // Update calibration (drag corners)
    RM_UpdateCalibration(&app->surf.calibration);
}

//------------------------------------------------------------------------------
// AppUpdate
//------------------------------------------------------------------------------

void AppUpdate(AppState *app, float dt)
{
    if (!app) return;

    if (app->mode == MODE_PRERECORDED) {
        RMV_UpdateVideo(app->surf.videoPrerecorded, dt);
    } else {
        RMV_UpdateVideo(app->surf.videoBg, dt);
        RMV_UpdateVideo(app->surf.videoFg, dt);
    }
}

//------------------------------------------------------------------------------
// AppDraw
//------------------------------------------------------------------------------

void AppDraw(AppState *app, float dt)
{
    if (!app) return;

    // --- Dessin dans la surface ---
    RM_BeginSurface(app->surf.surface);
        ClearBackground(BLACK);

        if (app->mode == MODE_PRERECORDED) {
            // Vidéo pré-enregistrée plein cadre
            DrawTexture(RMV_GetVideoTexture(app->surf.videoPrerecorded), 0, 0, WHITE);

        } else {
            // Fond
            DrawTexture(RMV_GetVideoTexture(app->surf.videoBg), 0, 0, WHITE);

            // Avant — opacité pilotée
            unsigned char alpha = (unsigned char)(app->surf.fgOpacity * 255.0f);
            DrawTexture(RMV_GetVideoTexture(app->surf.videoFg), 0, 0, (Color){ 255, 255, 255, alpha });

            // Contenu temps réel additionnel (render.c)
            DrawRealtime(app, dt);
        }

    RM_EndSurface(app->surf.surface);

    // --- Dessin à l'écran ---
    BeginDrawing();
        ClearBackground(BLACK);
        RM_DrawSurface(app->surf.surface);
        RM_DrawCalibration(app->surf.calibration);
        if (app->hudVisible) DrawHUD(app);
    EndDrawing();
}
