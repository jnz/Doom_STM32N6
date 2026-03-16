//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#include "config.h"

// #include "deh_str.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_timer.h"
#include "i_video.h"

#include "i_system.h"

#include "w_wad.h"
#include "z_zone.h"

void I_AtExit(atexit_func_t func, boolean run_on_error)
{

}

// Tactile feedback function, probably used for the Logitech Cyberman
void I_Tactile(int on, int off, int total)
{

}

// Zone memory auto-allocation function that allocates the zone size
// by trying progressively smaller zone sizes until one is found that
// works.
byte *I_ZoneBase (int *size)
{
    *size = 0x800000;
    byte* zone = (byte*)malloc(*size);
    if (zone == NULL)
    {
    	I_Error("Zone Malloc Failed!");
    }
    return zone;
}

void I_PrintBanner(char *msg)
{
    printf("%s\n", msg);
}

void I_PrintDivider(void)
{

}

void I_PrintStartupBanner(char *gamedescription)
{
    printf("%s\n", gamedescription);
}

//
// I_ConsoleStdout
//
// Returns true if stdout is a real console, false if it is a file
//
boolean I_ConsoleStdout(void)
{
    return true;
}

void I_Quit (void)
{
}

// moved to stm32 main.c
#if 0
void I_Error (char *error, ...)
{
    while(1) {}
}
#endif

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    while(1) {} // Not implemented
    return false;
}

