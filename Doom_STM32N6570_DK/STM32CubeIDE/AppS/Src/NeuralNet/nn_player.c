// nn_player.c
// NN bot player with last-action feedback and stuck recovery.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nn_player.h"
#include "nn_common.h"
#include "nn_infer.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_fixed.h"
#include "d_event.h"

static const int fwd_map[3]  = { -25, 0, 25 };
static const int side_map[3] = { -20, 0, 20 };
static const int turn_map[5] = { -512, -192, 0, 192, 512 };

static boolean nn_active = false;
static float last_action[5] = { 1.0f, 1.0f, 2.0f, 0.0f, 0.0f };

boolean NN_PlayerInit(void)
{
    nn_active = true;
    last_action[0] = 1.0f;  // stop
    last_action[1] = 1.0f;  // none
    last_action[2] = 2.0f;  // straight
    last_action[3] = 0.0f;  // no fire
    last_action[4] = 0.0f;  // no use
    fprintf(stderr, "NN_Player: initialized (pure C, weights compiled in)\n");
    return true;
}

void NN_PlayerBuildTicCmd(player_t *player, ticcmd_t *cmd)
{
    nn_result_t result;
    float features[NN_NUM_FEATURES];

    if (!nn_active || !player->mo)
    {
        memset(cmd, 0, sizeof(*cmd));
        return;
    }

    // Build feature vector with last action feedback
    NN_BuildFeatures(player, last_action, features);

    // Run inference
    NN_Infer(features, &result);

    // Map to Doom commands
    memset(cmd, 0, sizeof(*cmd));
    cmd->forwardmove = fwd_map[result.fwd_class];
    cmd->sidemove    = side_map[result.side_class];
    cmd->angleturn   = turn_map[result.turn_class];
    cmd->buttons     = (result.fire ? BT_ATTACK : 0)
                     | (result.use  ? BT_USE    : 0);

    // --- Stuck detection & recovery ---
    static int stuck_count = 0;
    {
    static fixed_t last_x = 0, last_y = 0;
    static int recovery_timer = 0;  // bleibt in Recovery bis abgelaufen

    fixed_t dx = player->mo->x - last_x;
    fixed_t dy = player->mo->y - last_y;

    if (abs(dx) < 5 * FRACUNIT && abs(dy) < 5 * FRACUNIT)
        stuck_count++;
    else if (recovery_timer <= 0)
        stuck_count = 0;

    // Recovery starten
    if (stuck_count > 35 && recovery_timer <= 0)
        recovery_timer = 70;  // 2 Sekunden Recovery erzwingen

    if (recovery_timer > 0)
    {
        recovery_timer--;

        // Erste Hälfte: drehen zum offensten Ray
        // Zweite Hälfte: vorwärts laufen
        if (recovery_timer > 35)
        {
            int best_ray = 0;
            float best_dist = features[5];
            for (int i = 1; i < NN_NUM_RAYS; i++)
            {
                if (features[5 + i] > best_dist)
                {
                    best_dist = features[5 + i];
                    best_ray = i;
                }
            }

            // Etwas Zufall dazu damit nicht immer die gleiche Ecke
            best_ray = (best_ray + (rand() % 3) - 1) & 15;

            if (best_ray <= 8)
                cmd->angleturn = best_ray * 128;
            else
                cmd->angleturn = -(16 - best_ray) * 128;

            cmd->forwardmove = 0;
        }
        else
        {
            cmd->forwardmove = 25;
            cmd->angleturn = 0;
        }

        // stuck_count erst nach Recovery resetten
        if (recovery_timer == 0)
            stuck_count = 0;
    }

    last_x = player->mo->x;
    last_y = player->mo->y;
    }

    if (stuck_count > 10 &&
        cmd->forwardmove == 0 &&
        cmd->sidemove == 0 &&
        cmd->angleturn == 0)
    {
        cmd->forwardmove = 25;
    }

    // Update last_action for next tic
    last_action[0] = (float)result.fwd_class;
    last_action[1] = (float)result.side_class;
    last_action[2] = (float)result.turn_class;
    last_action[3] = (float)result.fire;
    last_action[4] = (float)result.use;

    // Debug output once per second
    {
        static int dbg_count = 0;
        if (dbg_count++ % 35 == 0)
        {
            fprintf(stderr, "NN: fwd=%d side=%d turn=%d fire=%d use=%d "
                            "| ray0=%.0f ray4=%.0f ray8=%.0f stuck: %i\n",
                    cmd->forwardmove, cmd->sidemove, cmd->angleturn,
                    result.fire, result.use,
                    features[5], features[9], features[13], stuck_count);
        }
    }
}

void NN_PlayerShutdown(void)
{
    nn_active = false;
    fprintf(stderr, "NN_Player: shutdown\n");
}
