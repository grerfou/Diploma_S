/*******************************************************************************
*
*   proj1 — main.c
*   Point d'entrée. Boucle principale.
*
*******************************************************************************/

#include "app.h"

int main(void)
{
    AppState app = { 0 };

    if (!AppInit(&app)) {
        TraceLog(LOG_ERROR, "APP: Initialization failed, aborting");
        AppShutdown(&app);
        return 1;
    }

    // Boucle principale
    while (app.running) {
        float dt = GetFrameTime();
        AppHandleInput(&app);
        AppUpdate(&app, dt);
        AppDraw(&app, dt);
    }

    AppShutdown(&app);
    return 0;
}
