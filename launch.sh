#!/bin/bash
#==============================================================================
#   launch.sh — Démarrage synchronisé de proj1 et proj2
#
#   USAGE:
#       ./launch.sh  (depuis mapping_app/)
#
#   Les fenêtres s'ouvrent et attendent.
#   Appuie sur ESPACE dans ce terminal pour démarrer les vidéos simultanément.
#==============================================================================

set -e

PROJ1_BIN="proj1/bin/proj1"
PROJ2_BIN="proj2/bin/proj2"

# Vérification des binaires
if [ ! -f "$PROJ1_BIN" ]; then
    echo "❌ $PROJ1_BIN non trouvé — lance 'make' dans proj1/"
    exit 1
fi

if [ ! -f "$PROJ2_BIN" ]; then
    echo "❌ $PROJ2_BIN non trouvé — lance 'make' dans proj2/"
    exit 1
fi

# Nettoyage shared memory résiduelle
rm -f /dev/shm/raymap_sync 2>/dev/null || true

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  MAPPING APP — Démarrage synchronisé"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Compilation du launcher si nécessaire
LAUNCHER_BIN="/tmp/raymap_launcher"

if [ ! -f "$LAUNCHER_BIN" ] || [ "shared/sync.c" -nt "$LAUNCHER_BIN" ]; then
    echo "→ Compilation du launcher de synchronisation..."
    gcc -O1 -o "$LAUNCHER_BIN" \
        -x c - \
        shared/sync.c \
        -I shared/ \
        -lrt << 'LAUNCHER_EOF'
#define _DEFAULT_SOURCE
#include "sync.h"
#include <stdio.h>
#include <time.h>

int main(void) {
    SyncHandle *sync = SyncOpen(SYNC_ROLE_LAUNCHER);
    if (!sync) {
        fprintf(stderr, "LAUNCHER: Failed to open sync\n");
        return 1;
    }

    // Attendre que les deux programmes soient prêts
    if (!SyncWaitReady(sync)) {
        fprintf(stderr, "LAUNCHER: Timeout — un des programmes n'a pas répondu\n");
        SyncClose(sync, true);
        return 1;
    }

    // Attendre la touche ESPACE
    printf("\n");
    printf("┌─────────────────────────────────────────┐\n");
    printf("│  ✓ Les deux programmes sont prêts       │\n");
    printf("│  → Appuie sur ESPACE pour démarrer      │\n");
    printf("└─────────────────────────────────────────┘\n");
    printf("\n");
    fflush(stdout);

    int c;
    while ((c = getchar()) != ' ') {
        if (c == EOF || c == 'q') {
            fprintf(stderr, "LAUNCHER: Annulé\n");
            SyncClose(sync, true);
            return 1;
        }
    }

    // 50ms pour s'assurer que les deux sont bien en attente du GO
    struct timespec ts = { 0, 50000000L };
    nanosleep(&ts, NULL);

    // Envoyer le GO
    SyncSendGo(sync);
    SyncClose(sync, false);

    printf("✓ GO envoyé — les deux programmes démarrent simultanément\n");
    return 0;
}
LAUNCHER_EOF
    echo "✓ Launcher compilé"
fi

# Lancement des programmes depuis leur propre dossier
echo "→ Lancement proj1 (projecteur 1)..."
(cd proj1 && ./bin/proj1) &
PID1=$!

echo "→ Lancement proj2 (projecteur 2)..."
(cd proj2 && ./bin/proj2) &
PID2=$!

echo "→ PID proj1: $PID1"
echo "→ PID proj2: $PID2"
echo ""
echo "→ Initialisation en cours..."
echo ""

# Lancer le launcher de synchronisation
"$LAUNCHER_BIN"
SYNC_STATUS=$?

if [ $SYNC_STATUS -ne 0 ]; then
    echo "❌ Synchronisation échouée — arrêt des programmes"
    kill $PID1 $PID2 2>/dev/null || true
    exit 1
fi

echo ""
echo "✓ Les deux programmes sont synchronisés et en cours d'exécution"
echo "  Ferme les fenêtres ou Ctrl+C pour tout arrêter"
echo ""

# Ctrl+C propre
cleanup() {
    echo ""
    echo "→ Arrêt des programmes..."
    kill $PID1 $PID2 2>/dev/null || true
    wait $PID1 $PID2 2>/dev/null || true
    rm -f /dev/shm/raymap_sync 2>/dev/null || true
    echo "✓ Arrêt complet"
    exit 0
}
trap cleanup SIGINT SIGTERM

# Attendre la fin des deux programmes
wait $PID1 $PID2
echo "✓ Les deux programmes se sont terminés"
