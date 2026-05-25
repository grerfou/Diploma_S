/*******************************************************************************
*
*   proj1 — hud.c
*   Overlay régie — affiché par-dessus la surface mappée.
*   Toggle avec la touche H.
*
*******************************************************************************/

#include "app.h"

void DrawHUD(const AppState *app)
{
    if (!app) return;

    // Fond semi-transparent bas-gauche
    DrawRectangle(0, APP_SCREEN_HEIGHT - 120, 420, 120, (Color){ 0, 0, 0, 180 });

    // Mode actif
    const char *modeStr = (app->mode == MODE_PRERECORDED) ? "PRE-RECORDED" : "REALTIME";
    Color modeColor     = (app->mode == MODE_PRERECORDED) ? SKYBLUE : GREEN;
    DrawText(TextFormat("MODE : %s", modeStr), 10, APP_SCREEN_HEIGHT - 110, 20, modeColor);

    // Calibration
    const char *calibStr = app->surf.calibration.enabled ? "ON" : "OFF";
    Color calibColor     = app->surf.calibration.enabled ? YELLOW : GRAY;
    DrawText(TextFormat("CALIB : %s", calibStr), 10, APP_SCREEN_HEIGHT - 82, 20, calibColor);

    // Opacité fg (temps réel uniquement)
    if (app->mode == MODE_REALTIME) {
        DrawText(TextFormat("FG OPACITY : %.0f%%", app->surf.fgOpacity * 100.0f),
                 10, APP_SCREEN_HEIGHT - 54, 20, WHITE);
    }

    // FPS
    DrawText(TextFormat("FPS : %d", GetFPS()), 10, APP_SCREEN_HEIGHT - 26, 20, LIGHTGRAY);

    // Raccourcis clavier — coin haut-gauche, très discret
    DrawText("[TAB] Mode  [C] Calib  [R] Reset  [S] Save  [L] Load  [H] HUD",
             10, 10, 16, (Color){ 255, 255, 255, 100 });
}
