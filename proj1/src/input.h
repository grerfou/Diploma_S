#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

// Port série de l'ESP — à adapter selon le système
// Linux : /dev/ttyUSB0 ou /dev/ttyACM0
// Vérifier avec : ls /dev/tty*
#define INPUT_SERIAL_PORT   "/dev/ttyUSB0"
#define INPUT_BAUD_RATE     115200

//------------------------------------------------------------------------------
// Fonctions — input.c
//------------------------------------------------------------------------------

// Ouvre le port série — retourne false si échec
bool InputInit(const char *port, int baudRate);

// Ferme le port série
void InputShutdown(void);

// Lit les données disponibles — retourne true si une nouvelle valeur est reçue
// La valeur est normalisée [0.0 - 1.0]
bool InputRead(float *value);

// Retourne true si le port série est ouvert
bool InputIsConnected(void);

#endif // INPUT_H
