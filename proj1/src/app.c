/*******************************************************************************
*
*   proj1 — app.c
*   Initialisation, gestion des inputs, update et draw de l'application.
*
*******************************************************************************/
#include "input.h"
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

    // --- Vidéos en premier ---
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

    // --- Surface avec dimensions de la vidéo ---
    RMV_VideoInfo info = RMV_GetVideoInfo(app->surf.videoPrerecorded);
    app->surf.surface = RM_CreateSurface(info.width, info.height, RM_MAP_HOMOGRAPHY);
    if (!app->surf.surface) {
        TraceLog(LOG_ERROR, "APP: Failed to create surface");
        return false;
    }

    // Charge la calibration ou recentre par défaut
    if (!RM_LoadConfig(app->surf.surface, APP_CONFIG)) {
        TraceLog(LOG_WARNING, "APP: No config found, using default quad");
        //RM_ResetQuad(app->surf.surface, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT - 500);
        
        float scale = (float)APP_SCREEN_HEIGHT / (float)info.height;
        float w = info.width * scale;
        float h = (float)APP_SCREEN_HEIGHT;
        float x = (APP_SCREEN_WIDTH - w) / 2.0f;

        RM_SetQuad(app->surf.surface, (RM_Quad){
             { x,     0.0f },
             { x + w, 0.0f },
             { x + w, h    },
             { x,     h    }
        });
    }

    // Calibration
    app->surf.calibration = RM_CalibrationDefault(app->surf.surface);
    app->surf.calibration.enabled = false;

    // Opacité fg par défaut
    app->surf.fgOpacity = 1.0f;
    // Speed fg par défault
    app->surf.fgSpeed = 1.0f;
    
    // Toutes les vidéos en boucle
    RMV_SetVideoLoop(app->surf.videoPrerecorded, false);
    RMV_SetVideoLoop(app->surf.videoBg, true);
    RMV_SetVideoLoop(app->surf.videoFg, true);

    // Démarre la vidéo pré-enregistrée (mode par défaut)
    RMV_PlayVideo(app->surf.videoPrerecorded);

    TraceLog(LOG_INFO, "APP: Initialized [surface %dx%d]", info.width, info.height);
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

    // Opacité fg — touches + et -
    if (IsKeyDown(KEY_UP)) {
        app->surf.fgOpacity += 0.01f;
        if (app->surf.fgOpacity > 1.0f) app->surf.fgOpacity = 1.0f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        app->surf.fgOpacity -= 0.01f;
        if (app->surf.fgOpacity < 0.0f) app->surf.fgOpacity = 0.0f;
    }

    // Speed fg 
    if (IsKeyDown(KEY_RIGHT)) {
        app->surf.fgSpeed = fminf(app->surf.fgSpeed + 0.01f, 3.0f);
        RMV_SetVideoSpeed(app->surf.videoFg, app->surf.fgSpeed);
    }
    if (IsKeyDown(KEY_LEFT)) {
        app->surf.fgSpeed = fmaxf(app->surf.fgSpeed - 0.01f, 0.0f);
        RMV_SetVideoSpeed(app->surf.videoFg, app->surf.fgSpeed);
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

    // Switch auto vers REALTIME quand la vidéo pré-enregistrée est terminée
    if (app->mode == MODE_PRERECORDED &&
        RMV_GetVideoState(app->surf.videoPrerecorded) == RMV_STATE_STOPPED) {
        RMV_PlayVideo(app->surf.videoBg);
        RMV_PlayVideo(app->surf.videoFg);
        app->mode = MODE_REALTIME;
        TraceLog(LOG_INFO, "APP: Auto-switch to REALTIME mode");
    }

    // Lecture donnée esp
    float espVal;
    if (InputRead(&espVal)) {
        app->espValue = espVal;
        app->surf.fgOpacity = espVal;
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
        ClearBackground(BLANK);

        if (app->mode == MODE_PRERECORDED) {
            // Vidéo pré-enregistrée plein cadre
            DrawTexture(RMV_GetVideoTexture(app->surf.videoPrerecorded), 0, 0, WHITE);

        } else {
            // Fond
            //DrawTexture(RMV_GetVideoTexture(app->surf.videoBg), 0, 0, WHITE);

            // Avant — opacité pilotée
            unsigned char alphaFg = (unsigned char)(app->surf.fgOpacity * 255.0f);
            //unsigned char alphaBg = (unsigned char)((1.0f - app->surf.fgOpacity) * 255.0f + 100);
            //unsigned char alphaBg = 255;

            BeginBlendMode(BLEND_ALPHA);
            //DrawTexture(RMV_GetVideoTexture(app->surf.videoBg), 0, 0, (Color){ 255, 255, 255, alphaBg });
            DrawTexture(RMV_GetVideoTexture(app->surf.videoBg), 0, 0, WHITE );
            DrawTexture(RMV_GetVideoTexture(app->surf.videoFg), 0, 0, (Color){ 255, 255, 255, alphaFg });
            EndBlendMode();

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
