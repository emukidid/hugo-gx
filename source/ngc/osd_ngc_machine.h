/****************************************************************************
 * Hu-Go! Nintendo Gamecube/wii
 *
 * machine functions
 ****************************************************************************/

#include "pce.h"

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <zlib.h>
#include <sys/dir.h>
#include <sys/unistd.h>

#define MAXPATHLEN 512
#define DEFAULT_PATH "/hugo"

extern bool fat_enabled;
extern void WaitPrompt( char *prompt );
extern void ShowAction( char *text );
extern void ClearScreen();
extern void SetScreen();
extern u16 ogc_input__getMenuButtons(void);
