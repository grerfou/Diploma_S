/*******************************************************************************
*
*   proj1 — input.c
*
*   Réception des données depuis l'ESP via port série.
*   L'ESP envoie une valeur normalisée [0.0 - 1.0] qui contrôle
*   l'opacité et la vitesse de lecture de la vidéo fg.
*
*   Format attendu : à définir avec le collègue
*   Options probables :
*       - Float ASCII  : "0.75\n"
*       - Int 0-255    : "192\n"  → divisé par 255.0
*       - JSON         : "{\"v\":0.75}\n"
*
*   TODO : adapter InputRead() selon le format défini
*
*******************************************************************************/
#define _DEFAULT_SOURCE

#include "input.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

//------------------------------------------------------------------------------
// État interne
//------------------------------------------------------------------------------

static int  s_fd          = -1;   // File descriptor du port série
static char s_buf[256]    = {0};  // Buffer de lecture
static int  s_bufLen      = 0;    // Longueur du buffer

//------------------------------------------------------------------------------
// InputInit
//------------------------------------------------------------------------------

bool InputInit(const char *port, int baudRate)
{
    s_fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (s_fd < 0) {
        TraceLog(LOG_WARNING, "INPUT: Cannot open serial port '%s' : %s", port, strerror(errno));
        return false;
    }

    struct termios tty;
    if (tcgetattr(s_fd, &tty) != 0) {
        TraceLog(LOG_ERROR, "INPUT: tcgetattr failed : %s", strerror(errno));
        close(s_fd);
        s_fd = -1;
        return false;
    }

    // Baudrate
    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:
            TraceLog(LOG_WARNING, "INPUT: Unknown baud rate %d, using 115200", baudRate);
            speed = B115200;
            break;
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Config 8N1 — mode raw
    cfmakeraw(&tty);
    tty.c_cc[VMIN]  = 0;   // Non-bloquant
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(s_fd, TCSANOW, &tty) != 0) {
        TraceLog(LOG_ERROR, "INPUT: tcsetattr failed : %s", strerror(errno));
        close(s_fd);
        s_fd = -1;
        return false;
    }

    TraceLog(LOG_INFO, "INPUT: Serial port '%s' opened at %d baud", port, baudRate);
    return true;
}

//------------------------------------------------------------------------------
// InputShutdown
//------------------------------------------------------------------------------

void InputShutdown(void)
{
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
        TraceLog(LOG_INFO, "INPUT: Serial port closed");
    }
}

//------------------------------------------------------------------------------
// InputIsConnected
//------------------------------------------------------------------------------

bool InputIsConnected(void)
{
    return (s_fd >= 0);
}

//------------------------------------------------------------------------------
// InputRead
//------------------------------------------------------------------------------

bool InputRead(float *value)
{
    if (s_fd < 0 || !value) return false;

    // Lire les bytes disponibles
    char tmp[64];
    int n = (int)read(s_fd, tmp, sizeof(tmp) - 1);
    if (n <= 0) return false;

    // Accumuler dans le buffer jusqu'à '\n'
    for (int i = 0; i < n; i++) {
        if (s_bufLen < (int)sizeof(s_buf) - 1) {
            s_buf[s_bufLen++] = tmp[i];
        }

        if (tmp[i] == '\n') {
            s_buf[s_bufLen] = '\0';

            // TODO : adapter le parsing selon le format défini avec le collègue
            // Option A — Float ASCII "0.75\n"
            float parsed = atof(s_buf);
            if (parsed < 0.0f) parsed = 0.0f;
            if (parsed > 1.0f) parsed = 1.0f;
            *value = parsed;

            // Reset buffer
            s_bufLen = 0;
            memset(s_buf, 0, sizeof(s_buf));
            return true;
        }
    }

    return false;
}
