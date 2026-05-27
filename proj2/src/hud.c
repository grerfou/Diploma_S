/*******************************************************************************
*
*   proj2 — hud.c
*   Overlay régie — affiché par-dessus les surfaces mappées.
*   Toggle avec la touche H.
*
*******************************************************************************/

#include "app.h"

void DrawHUD(const AppState *app)
{
    if (!app) return;

    // Fond semi-transparent bas-gauche
    DrawRectangle(0, APP_SCREEN_HEIGHT - 150, 480, 150, (Color){ 0, 0, 0, 180 });

    // Mode actif
    const char *modeStr = (app->mode == MODE_PRERECORDED) ? "PRE-RECORDED" : "REALTIME";
    Color modeColor     = (app->mode == MODE_PRERECORDED) ? SKYBLUE : GREEN;
    DrawText(TextFormat("MODE : %s", modeStr), 10, APP_SCREEN_HEIGHT - 140, 20, modeColor);

    // Surface active
    const char *surfStr = (app->activeSurface == SURFACE_A) ? "A" : "B";
    DrawText(TextFormat("SURFACE : %s  [1] A  [2] B", surfStr),
             10, APP_SCREEN_HEIGHT - 112, 20, ORANGE);

    // Calibration surface active
    const RM_Calibration *calib = &app->surfaces[app->activeSurface].calibration;
    const char *calibStr = calib->enabled ? "ON" : "OFF";
    Color calibColor     = calib->enabled ? YELLOW : GRAY;
    DrawText(TextFormat("CALIB : %s", calibStr), 10, APP_SCREEN_HEIGHT - 84, 20, calibColor);

    // Opacité et vitesse fg surface active
    const SurfaceState *active = &app->surfaces[app->activeSurface];
    if (app->mode == MODE_REALTIME) {
        DrawText(TextFormat("FG OPACITY : %.0f%%", active->fgOpacity * 100.0f),
                 10, APP_SCREEN_HEIGHT - 56, 20, WHITE);
        DrawText(TextFormat("FG SPEED : %.2fx", active->fgSpeed),
                 240, APP_SCREEN_HEIGHT - 56, 20, WHITE);
    }

    // FPS
    DrawText(TextFormat("FPS : %d", GetFPS()), 10, APP_SCREEN_HEIGHT - 28, 20, LIGHTGRAY);

    // Raccourcis
    DrawText("[TAB] Mode  [1/2] Surface  [C] Calib  [R] Reset  [S] Save  [L] Load  [H] HUD",
             10, 10, 16, (Color){ 255, 255, 255, 100 });
}
