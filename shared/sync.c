/*******************************************************************************
*
*   sync.c — Implémentation synchronisation via shared memory POSIX
*
*******************************************************************************/

#define _DEFAULT_SOURCE

#include "sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

//------------------------------------------------------------------------------
// Structure interne
//------------------------------------------------------------------------------

struct SyncHandle {
    SyncState *state;   // Pointeur vers la shared memory
    SyncRole   role;    // Rôle de ce processus
    int        fd;      // File descriptor de la shared memory
};

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

// Retourne le temps en millisecondes
static long sync_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

//------------------------------------------------------------------------------
// SyncOpen
//------------------------------------------------------------------------------

SyncHandle *SyncOpen(SyncRole role)
{
    SyncHandle *handle = (SyncHandle *)malloc(sizeof(SyncHandle));
    if (!handle) {
        fprintf(stderr, "SYNC: malloc failed\n");
        return NULL;
    }

    handle->role = role;

    // Créer ou ouvrir la shared memory
    int flags = O_RDWR | O_CREAT;
    handle->fd = shm_open(SYNC_SHM_NAME, flags, 0666);
    if (handle->fd < 0) {
        perror("SYNC: shm_open failed");
        free(handle);
        return NULL;
    }

    // Dimensionner la shared memory
    if (ftruncate(handle->fd, sizeof(SyncState)) < 0) {
        perror("SYNC: ftruncate failed");
        close(handle->fd);
        free(handle);
        return NULL;
    }

    // Mapper en mémoire
    handle->state = (SyncState *)mmap(
        NULL,
        sizeof(SyncState),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        handle->fd,
        0
    );

    if (handle->state == MAP_FAILED) {
        perror("SYNC: mmap failed");
        close(handle->fd);
        free(handle);
        return NULL;
    }

    fprintf(stderr, "SYNC: Opened [role=%d]\n", role);
    return handle;
}

//------------------------------------------------------------------------------
// SyncReset
//------------------------------------------------------------------------------

void SyncReset(SyncHandle *sync)
{
    if (!sync || !sync->state) return;
    sync->state->ready1 = 0;
    sync->state->ready2 = 0;
    sync->state->go     = 0;
    fprintf(stderr, "SYNC: Reset\n");
}

//------------------------------------------------------------------------------
// SyncSignalReady
//------------------------------------------------------------------------------

void SyncSignalReady(SyncHandle *sync)
{
    if (!sync || !sync->state) return;

    if (sync->role == SYNC_ROLE_PROJ1) {
        sync->state->ready1 = 1;
        fprintf(stderr, "SYNC: proj1 READY\n");
    } else if (sync->role == SYNC_ROLE_PROJ2) {
        sync->state->ready2 = 1;
        fprintf(stderr, "SYNC: proj2 READY\n");
    }
}

//------------------------------------------------------------------------------
// SyncWaitGo
//------------------------------------------------------------------------------

bool SyncWaitGo(SyncHandle *sync)
{
    if (!sync || !sync->state) return false;

    long start = sync_now_ms();
    fprintf(stderr, "SYNC: Waiting for GO...\n");

    while (!sync->state->go) {
        // Vérifier le timeout
        if (sync_now_ms() - start > SYNC_TIMEOUT_MS) {
            fprintf(stderr, "SYNC: Timeout waiting for GO\n");
            return false;
        }
        // Attendre 1ms avant de revérifier
        struct timespec ts = { 0, 1000000L };
        nanosleep(&ts, NULL);
    }

    fprintf(stderr, "SYNC: GO received!\n");
    return true;
}

//------------------------------------------------------------------------------
// SyncWaitReady
//------------------------------------------------------------------------------

bool SyncWaitReady(SyncHandle *sync)
{
    if (!sync || !sync->state) return false;

    long start = sync_now_ms();
    fprintf(stderr, "SYNC: Waiting for proj1 and proj2 to be READY...\n");

    while (!sync->state->ready1 || !sync->state->ready2) {
        if (sync_now_ms() - start > SYNC_TIMEOUT_MS) {
            fprintf(stderr, "SYNC: Timeout — ready1=%d ready2=%d\n",
                    sync->state->ready1, sync->state->ready2);
            return false;
        }
        struct timespec ts = { 0, 1000000L };
        nanosleep(&ts, NULL);
    }

    fprintf(stderr, "SYNC: Both programs READY\n");
    return true;
}

//------------------------------------------------------------------------------
// SyncSendGo
//------------------------------------------------------------------------------

void SyncSendGo(SyncHandle *sync)
{
    if (!sync || !sync->state) return;
    sync->state->go = 1;
    fprintf(stderr, "SYNC: GO sent!\n");
}

//------------------------------------------------------------------------------
// SyncClose
//------------------------------------------------------------------------------

void SyncClose(SyncHandle *sync, bool destroy)
{
    if (!sync) return;

    if (sync->state && sync->state != MAP_FAILED) {
        munmap(sync->state, sizeof(SyncState));
    }

    if (sync->fd >= 0) {
        close(sync->fd);
    }

    if (destroy) {
        shm_unlink(SYNC_SHM_NAME);
        fprintf(stderr, "SYNC: Shared memory destroyed\n");
    }

    free(sync);
}
