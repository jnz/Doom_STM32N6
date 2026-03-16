// nn_common.h
// Shared NN functions for Doom bot

#ifndef NN_COMMON_H
#define NN_COMMON_H

#include "d_player.h"
#include "m_fixed.h"

#define NN_NUM_RAYS        16
#define NN_MAX_MONSTERS     3
#define NN_NUM_FEATURES    35   // 5 player + 16 rays + 9 monsters + 5 last_action
#define NN_RAY_MAX_DIST    (2048 * FRACUNIT)

typedef struct {
    float dx;        // relative x, rotated into player frame
    float dy;        // relative y, rotated into player frame
    float present;   // 1.0 if monster exists, 0.0 if padding
    float dist;      // for sorting (not stored in features)
} nn_monster_t;

float NN_CastRay(fixed_t x, fixed_t y, angle_t angle, fixed_t max_dist);
void  NN_FindNearestMonsters(player_t *player, nn_monster_t *out, int max_n);

// Build feature vector. last_action[5] = {fwd,side,turn,fire,use} from previous tic.
void  NN_BuildFeatures(player_t *player, const float *last_action, float *features);

#endif
