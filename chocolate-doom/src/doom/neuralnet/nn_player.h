// nn_player.h
// Neural network bot player for Choco Doom
// Pure C inference - no external dependencies.

#ifndef NN_PLAYER_H
#define NN_PLAYER_H

#include "doomtype.h"
#include "d_player.h"
#include "d_ticcmd.h"

boolean NN_PlayerInit(void);
void NN_PlayerBuildTicCmd(player_t *player, ticcmd_t *cmd);
void NN_PlayerShutdown(void);

#endif
