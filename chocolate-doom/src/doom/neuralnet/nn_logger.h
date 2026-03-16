// nn_logger.h
// Neural network training data logger for Choco Doom

#ifndef NN_LOGGER_H
#define NN_LOGGER_H

#include "d_player.h"
#include "d_ticcmd.h"

void NN_InitLogger(void);
void NN_LogTic(player_t *player, ticcmd_t *cmd);
void NN_CloseLogger(void);

#endif
