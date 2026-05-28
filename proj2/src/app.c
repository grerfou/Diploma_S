/*******************************************************************************
*
*   proj2 — app.c
*   Initialisation, gestion des inputs, update et draw.
*   2 surfaces côte à côte sur un seul projecteur 1920x1080.
*
*******************************************************************************/
#include "input.h"
#include "app.h"
#include <stdio.h>

//------------------------------------------------------------------------------
// Helpers internes
//------------------------------------------------------------------------------

// Retourne la config path selon l'index
static const char *surf_config(int i) {
    return (i == 0) ? APP_CONFIG_A : APP_CONFIG_B;
}

//------------------------------------------------------------------------------
// AppInit
//------------------------------------------------------------------------------

bool AppInit(AppState *app)
{
    if (!app) return false;

    // Fenêtre
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    InitWindow(APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, "proj2");
    SetWindowPosition(4480, 0);  // Projecteur 2 — écran à droite
    SetTargetFPS(APP_TARGET_FPS);

    // État initial
    app->mode          = MODE_PRERECORDED;
    app->activeSurface = SURFACE_A;
    app->hudVisible    = true;
    app->running       = true;

    // --- Vidéo pré-enregistrée partagée ---
    // Une seule instance — les deux surfaces lisent la même vidéo
    RMV_Video *prerecorded = RMV_LoadVideo(APP_VIDEO_PRERECORDED);
    if (!prerecorded) {
        TraceLog(LOG_ERROR, "APP: Failed to load prerecorded video");
        return false;
    }
    RMV_SetVideoLoop(prerecorded, false);

    // --- Initialisation des 2 surfaces ---
    const char *bgPaths[2] = { APP_VIDEO_RT_BG_A, APP_VIDEO_RT_BG_B };
    const char *fgPaths[2] = { APP_VIDEO_RT_FG_A, APP_VIDEO_RT_FG_B };

    RMV_VideoInfo info = RMV_GetVideoInfo(prerecorded);

    for (int i = 0; i < 2; i++) {
        SurfaceState *s = &app->surfaces[i];

        // Vidéo pré-enregistrée partagée
        s->videoPrerecorded = prerecorded;

        // Vidéos temps réel
        s->videoBg = RMV_LoadVideo(bgPaths[i]);
        if (!s->videoBg) {
            TraceLog(LOG_ERROR, "APP: Failed to load RT_BG video for surface %d", i);
            return false;
        }

        s->videoFg = RMV_LoadVideo(fgPaths[i]);
        if (!s->videoFg) {
            TraceLog(LOG_ERROR, "APP: Failed to load RT_FG video for surface %d", i);
            return false;
        }

        RMV_SetVideoLoop(s->videoBg, true);
        RMV_SetVideoLoop(s->videoFg, true);

        // Surface raymap — dimensions de la vidéo pré-enregistrée
        // Chaque surface couvre la moitié de l'écran
        s->surface = RM_CreateSurface(info.width, info.height, RM_MAP_HOMOGRAPHY);
        if (!s->surface) {
            TraceLog(LOG_ERROR, "APP: Failed to create surface %d", i);
            return false;
        }

        // Charge la config ou quad par défaut (moitié gauche / droite)
        if (!RM_LoadConfig(s->surface, surf_config(i))) {
            TraceLog(LOG_WARNING, "APP: No config for surface %d, using default", i);

            // Surface A : moitié gauche, Surface B : moitié droite
            float scale = (float)APP_SCREEN_HEIGHT / (float)info.height;
            float w = info.width * scale;
            float h = (float)APP_SCREEN_HEIGHT;
            float x = (i == 0) ? 0.0f : (float)(APP_SCREEN_WIDTH / 2);

            // Centrer dans sa moitié
            float halfW = (float)(APP_SCREEN_WIDTH / 2);
            float offsetX = x + (halfW - w) / 2.0f;

            RM_SetQuad(s->surface, (RM_Quad){
                { offsetX,     0.0f },
                { offsetX + w, 0.0f },
                { offsetX + w, h    },
                { offsetX,     h    }
            });
        }

        // Calibration
        s->calibration = RM_CalibrationDefault(s->surface);
        s->calibration.enabled = false;

        // Paramètres RT
        s->fgOpacity = 1.0f;
        s->fgSpeed   = 1.0f;
    }

    // Synchronisation démarrage
    SyncHandle *sync = SyncOpen(SYNC_ROLE_PROJ2); // PROJ2 pour proj2
    if (sync) {
        SyncSignalReady(sync);
        SyncWaitGo(sync);
        SyncClose(sync, false);
    }

    // Démarre la vidéo pré-enregistrée
    RMV_PlayVideo(prerecorded);

    TraceLog(LOG_INFO, "APP: proj2 initialized [2 surfaces %dx%d]", info.width, info.height);
    return true;
}

//------------------------------------------------------------------------------
// AppShutdown
//------------------------------------------------------------------------------

void AppShutdown(AppState *app)
{
    if (!app) return;

    // Sauvegarde les calibrations
    for (int i = 0; i < 2; i++) {
        SurfaceState *s = &app->surfaces[i];
        if (s->surface) RM_SaveConfig(s->surface, surf_config(i));
    }

    // Vidéo pré-enregistrée partagée — unload une seule fois
    if (app->surfaces[0].videoPrerecorded) {
        RMV_UnloadVideo(app->surfaces[0].videoPrerecorded);
        app->surfaces[0].videoPrerecorded = NULL;
        app->surfaces[1].videoPrerecorded = NULL;
    }

    // Vidéos RT et surfaces
    for (int i = 0; i < 2; i++) {
        SurfaceState *s = &app->surfaces[i];
        if (s->videoBg)  RMV_UnloadVideo(s->videoBg);
        if (s->videoFg)  RMV_UnloadVideo(s->videoFg);
        if (s->surface)  RM_DestroySurface(s->surface);
    }

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

    // Sélection surface active
    if (IsKeyPressed(KEY_ONE))   app->activeSurface = SURFACE_A;
    if (IsKeyPressed(KEY_TWO))   app->activeSurface = SURFACE_B;

    // Switch mode global
    if (IsKeyPressed(KEY_TAB)) {
        if (app->mode == MODE_PRERECORDED) {
            RMV_PauseVideo(app->surfaces[0].videoPrerecorded);
            for (int i = 0; i < 2; i++) {
                RMV_PlayVideo(app->surfaces[i].videoBg);
                RMV_PlayVideo(app->surfaces[i].videoFg);
            }
            app->mode = MODE_REALTIME;
            TraceLog(LOG_INFO, "APP: Switched to REALTIME");
        } else {
            for (int i = 0; i < 2; i++) {
                RMV_StopVideo(app->surfaces[i].videoBg);
                RMV_StopVideo(app->surfaces[i].videoFg);
            }
            RMV_PlayVideo(app->surfaces[0].videoPrerecorded);
            app->mode = MODE_PRERECORDED;
            TraceLog(LOG_INFO, "APP: Switched to PRERECORDED");
        }
    }

    // Calibration sur la surface active
    SurfaceState *active = &app->surfaces[app->activeSurface];

    if (IsKeyPressed(KEY_C)) RM_ToggleCalibration(&active->calibration);
    if (IsKeyPressed(KEY_R)) RM_ResetCalibrationQuad(&active->calibration, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT);
    if (IsKeyPressed(KEY_S)) RM_SaveConfig(active->surface, surf_config(app->activeSurface));
    if (IsKeyPressed(KEY_L)) RM_LoadConfig(active->surface, surf_config(app->activeSurface));

    // Opacité fg surface active
    if (IsKeyDown(KEY_UP)) {
        active->fgOpacity = fminf(active->fgOpacity + 0.01f, 1.0f);
    }
    if (IsKeyDown(KEY_DOWN)) {
        active->fgOpacity = fmaxf(active->fgOpacity - 0.01f, 0.0f);
    }

    // Vitesse fg surface active
    if (IsKeyDown(KEY_RIGHT)) {
        active->fgSpeed = fminf(active->fgSpeed + 0.01f, 3.0f);
        RMV_SetVideoSpeed(active->videoFg, active->fgSpeed);
    }
    if (IsKeyDown(KEY_LEFT)) {
        active->fgSpeed = fmaxf(active->fgSpeed - 0.01f, 0.0f);
        RMV_SetVideoSpeed(active->videoFg, active->fgSpeed);
    }

    // Update calibration surface active
    RM_UpdateCalibration(&active->calibration);
}

//------------------------------------------------------------------------------
// AppUpdate
//------------------------------------------------------------------------------

void AppUpdate(AppState *app, float dt)
{
    if (!app) return;

    if (app->mode == MODE_PRERECORDED) {
        RMV_UpdateVideo(app->surfaces[0].videoPrerecorded, dt);

        // Switch auto vers REALTIME en fin de vidéo
        if (RMV_GetVideoState(app->surfaces[0].videoPrerecorded) == RMV_STATE_STOPPED) {
            for (int i = 0; i < 2; i++) {
		RMV_StopVideo(app->surfaces[i].videoBg);   // ← ajouter
        	RMV_StopVideo(app->surfaces[i].videoFg);
                RMV_PlayVideo(app->surfaces[i].videoBg);
                RMV_PlayVideo(app->surfaces[i].videoFg);
            }
            app->mode = MODE_REALTIME;
            TraceLog(LOG_INFO, "APP: Auto-switch to REALTIME");
        }
    } else {
        for (int i = 0; i < 2; i++) {
            RMV_UpdateVideo(app->surfaces[i].videoBg, dt);
            RMV_UpdateVideo(app->surfaces[i].videoFg, dt);
        }
    }

    // Lecture données ESP
    float espVal;
    if (InputRead(&espVal)) {
        app->espValue = espVal;
        // Applique l'opacité aux deux surfaces
        for (int i = 0; i < 2; i++) {
            app->surfaces[i].fgOpacity = espVal;
        }
    }
}

//------------------------------------------------------------------------------
// AppDraw
//------------------------------------------------------------------------------

void AppDraw(AppState *app, float dt)
{
    if (!app) return;

    // --- Dessin dans chaque surface ---
    for (int i = 0; i < 2; i++) {
        SurfaceState *s = &app->surfaces[i];

        RM_BeginSurface(s->surface);
            ClearBackground(BLANK);

            if (app->mode == MODE_PRERECORDED) {
                BeginBlendMode(BLEND_ALPHA);
                DrawTexture(RMV_GetVideoTexture(s->videoPrerecorded), 0, 0, WHITE);
                EndBlendMode();

            } else {
                unsigned char alphaFg = (unsigned char)(s->fgOpacity * 255.0f);

                BeginBlendMode(BLEND_ALPHA);
                DrawTexture(RMV_GetVideoTexture(s->videoBg), 0, 0, WHITE);
                EndBlendMode();

                BeginBlendMode(BLEND_ALPHA);
                DrawTexture(RMV_GetVideoTexture(s->videoFg), 0, 0,
                            (Color){ 255, 255, 255, alphaFg });
                EndBlendMode();

                DrawRealtime(app, i, dt);
            }

        RM_EndSurface(s->surface);
    }

    // --- Dessin à l'écran ---
    BeginDrawing();
        ClearBackground(BLACK);

        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 2; i++) {
            RM_DrawSurface(app->surfaces[i].surface);
        }
        EndBlendMode();

        // Calibration sur la surface active uniquement
        RM_DrawCalibration(app->surfaces[app->activeSurface].calibration);

        // Bordure orange sur la surface active
        RM_Quad q = RM_GetQuad(app->surfaces[app->activeSurface].surface);
        DrawLineEx(q.topLeft,     q.topRight,    2, ORANGE);
        DrawLineEx(q.topRight,    q.bottomRight, 2, ORANGE);
        DrawLineEx(q.bottomRight, q.bottomLeft,  2, ORANGE);
        DrawLineEx(q.bottomLeft,  q.topLeft,     2, ORANGE);

        if (app->hudVisible) DrawHUD(app);
    EndDrawing();
}
