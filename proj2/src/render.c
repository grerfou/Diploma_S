/*******************************************************************************
*
*   proj2 — render.c
*
*   Contenu temps réel dessiné PAR-DESSUS les vidéos bg/fg.
*   C'est le seul fichier à modifier pour changer le contenu artistique
*   en phase temps réel.
*
*   surfaceIndex : 0 = surface A (gauche), 1 = surface B (droite)
*
*******************************************************************************/

#include "app.h"

void DrawRealtime(AppState *app, int surfaceIndex, float dt)
{
    (void)app;
    (void)surfaceIndex;
    (void)dt;

    // TODO : ajouter le contenu temps réel ici
    // surfaceIndex permet d'avoir du contenu différent par surface
    // Exemple :
    //   if (surfaceIndex == 0) DrawCircle(540, 960, 100, ColorAlpha(WHITE, 0.5f));
    //   if (surfaceIndex == 1) DrawText("LIVE", 100, 100, 60, RED);
}
