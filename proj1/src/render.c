/*******************************************************************************
*
*   proj1 — render.c
*
*   Contenu temps réel dessiné PAR-DESSUS les vidéos bg/fg.
*   C'est le seul fichier à modifier pour changer le contenu artistique
*   en phase temps réel.
*
*   Appelé depuis AppDraw() uniquement en MODE_REALTIME.
*
*******************************************************************************/

#include "app.h"

void DrawRealtime(AppState *app, float dt)
{
    // Silence warning unused pour l'instant
    (void)app;
    (void)dt;

    // TODO : ajouter le contenu temps réel ici
    // Exemples :
    //   DrawCircle(960, 540, 100, ColorAlpha(WHITE, 0.5f));
    //   DrawText("LIVE", 100, 100, 60, RED);
}
