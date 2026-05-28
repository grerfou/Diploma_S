#ifndef SYNC_H
#define SYNC_H

/*******************************************************************************
*
*   sync.h — Synchronisation démarrage via shared memory
*
*   PRINCIPE :
*       proj1 et proj2 s'initialisent indépendamment.
*       Quand chacun est prêt (fenêtre ouverte, vidéos chargées),
*       il signale READY et attend le GO de launch.sh.
*       launch.sh attend que les deux soient prêts puis envoie GO.
*       Les deux programmes démarrent la lecture vidéo simultanément.
*
*   USAGE dans proj1/proj2 :
*       SyncHandle *sync = SyncOpen(SYNC_ROLE_PROJ1);
*       // ... init ...
*       SyncSignalReady(sync);   // "je suis prêt"
*       SyncWaitGo(sync);        // attendre le GO
*       SyncClose(sync);
*       // → lancer RMV_PlayVideo ici
*
*   USAGE dans launch.sh :
*       Le script lance les deux programmes puis attend via sync_wait_go
*
*******************************************************************************/

#include <stdbool.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define SYNC_SHM_NAME   "/raymap_sync"   // Nom de la zone mémoire partagée
#define SYNC_TIMEOUT_MS  10000           // Timeout d'attente en ms (10s)

//------------------------------------------------------------------------------
// Rôles
//------------------------------------------------------------------------------

typedef enum {
    SYNC_ROLE_PROJ1 = 0,
    SYNC_ROLE_PROJ2 = 1,
    SYNC_ROLE_LAUNCHER = 2   // Utilisé par le programme C du launcher
} SyncRole;

//------------------------------------------------------------------------------
// Structure partagée (en shared memory)
//------------------------------------------------------------------------------

typedef struct {
    volatile int ready1;   // proj1 a fini son init
    volatile int ready2;   // proj2 a fini son init
    volatile int go;       // launcher a donné le GO
} SyncState;

//------------------------------------------------------------------------------
// Handle opaque
//------------------------------------------------------------------------------

typedef struct SyncHandle SyncHandle;

//------------------------------------------------------------------------------
// API
//------------------------------------------------------------------------------

// Ouvre ou crée la shared memory selon le rôle
// Retourne NULL en cas d'erreur
SyncHandle *SyncOpen(SyncRole role);

// Signale que ce programme est prêt
void SyncSignalReady(SyncHandle *sync);

// Attend le GO du launcher (bloquant, avec timeout)
// Retourne true si GO reçu, false si timeout
bool SyncWaitGo(SyncHandle *sync);

// Pour le launcher : attend que les deux programmes soient prêts
// Retourne true si les deux sont prêts, false si timeout
bool SyncWaitReady(SyncHandle *sync);

// Pour le launcher : envoie le GO
void SyncSendGo(SyncHandle *sync);

// Réinitialise la shared memory (à appeler au début de launch)
void SyncReset(SyncHandle *sync);

// Ferme et libère les ressources
// Si destroy=true, supprime la shared memory (à appeler par le launcher en fin)
void SyncClose(SyncHandle *sync, bool destroy);

#endif // SYNC_H
