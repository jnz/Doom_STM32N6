// nn_logger.c
// Logs gamestate + action per tic to CSV for training.

#include <stdio.h>
#include <stdlib.h>

#include "nn_logger.h"
#include "nn_common.h"
#include "doomstat.h"

static FILE *logfile = NULL;
static int tic_count = 0;

// Last action as class indices (matching training discretization)
static float last_action[5] = { 1.0f, 1.0f, 2.0f, 0.0f, 0.0f };
// defaults: fwd=stop(1), side=none(1), turn=straight(2), fire=no(0), use=no(0)

// Discretize cmd values to class indices (must match train_doom_bot.py)
static float discretize_fwd(int val)
{
    if (val < -10) return 0.0f;       // backward
    else if (val > 10) return 2.0f;   // forward
    else return 1.0f;                 // stop
}

static float discretize_side(int val)
{
    if (val < -10) return 0.0f;       // right
    else if (val > 10) return 2.0f;   // left
    else return 1.0f;                 // none
}

static float discretize_turn(int val)
{
    if (val < -384) return 0.0f;      // hard right
    else if (val < -64) return 1.0f;  // slight right
    else if (val > 384) return 4.0f;  // hard left
    else if (val > 64) return 3.0f;   // slight left
    else return 2.0f;                 // straight
}

void NN_InitLogger(void)
{
    char filename[64];
    snprintf(filename, sizeof(filename), "nn_training_%i_e%im%i_%d.csv", gamemode, gameepisode, gamemap, gametic);

    logfile = fopen(filename, "w");
    if (!logfile)
    {
        fprintf(stderr, "NN_Logger: failed to open %s\n", filename);
        return;
    }

    tic_count = 0;
    last_action[0] = 1.0f;  // reset
    last_action[1] = 1.0f;
    last_action[2] = 2.0f;
    last_action[3] = 0.0f;
    last_action[4] = 0.0f;

    // CSV header
    fprintf(logfile, "tic,"
        "angle,health,momx,momy,weapon,");

    for (int i = 0; i < NN_NUM_RAYS; i++)
        fprintf(logfile, "ray%d,", i);

    for (int i = 0; i < NN_MAX_MONSTERS; i++)
        fprintf(logfile, "m%d_dx,m%d_dy,m%d_present,", i, i, i);

    fprintf(logfile, "last_fwd,last_side,last_turn,last_fire,last_use,");
    fprintf(logfile, "cmd_fwd,cmd_side,cmd_turn,cmd_buttons\n");

    fprintf(stderr, "NN_Logger: started logging to %s\n", filename);
}

void NN_LogTic(player_t *player, ticcmd_t *cmd)
{
    if (!logfile || !player->mo)
        return;

    // Build feature vector
    float features[NN_NUM_FEATURES];
    NN_BuildFeatures(player, last_action, features);

    // Write CSV row
    fprintf(logfile, "%d,", tic_count);
    for (int i = 0; i < NN_NUM_FEATURES; i++)
    {
        if (i == 0)           fprintf(logfile, "%.4f,", features[i]);  // angle
        else if (i == 1)      fprintf(logfile, "%.0f,", features[i]);  // health
        else if (i <= 4)      fprintf(logfile, "%.2f,", features[i]);  // momx,momy,weapon
        else if (i < 21)      fprintf(logfile, "%.1f,", features[i]);  // rays
        else                  fprintf(logfile, "%.1f,", features[i]);  // monsters + last_action
    }

    fprintf(logfile, "%d,%d,%d,%d\n",
            cmd->forwardmove, cmd->sidemove,
            cmd->angleturn, cmd->buttons);

    // Console output once per second
    if (tic_count % 35 == 0)
    {
        char raybar[18];
        int pos = 0;

        for (int i = 15; i >= 9; i--)
        {
            float r = features[5 + i];
            if (r < 64) raybar[pos] = '#';
            else if (r < 200) raybar[pos] = '=';
            else if (r < 500) raybar[pos] = '-';
            else raybar[pos] = '.';
            pos++;
        }
        raybar[pos++] = '|';
        {
            float r = features[5];
            if (r < 64) raybar[pos] = '#';
            else if (r < 200) raybar[pos] = '=';
            else if (r < 500) raybar[pos] = '-';
            else raybar[pos] = '.';
        }
        pos++;
        raybar[pos++] = '|';
        for (int i = 1; i <= 7; i++)
        {
            float r = features[5 + i];
            if (r < 64) raybar[pos] = '#';
            else if (r < 200) raybar[pos] = '=';
            else if (r < 500) raybar[pos] = '-';
            else raybar[pos] = '.';
            pos++;
        }
        raybar[pos] = '\0';

        fprintf(stderr,
            "[%5d] hp:%3.0f | R%sL | "
            "m0:%.0f/%.0f | cmd: fwd=%d side=%d turn=%d btn=%d\n",
            tic_count, features[1],
            raybar,
            features[21], features[22],
            cmd->forwardmove, cmd->sidemove,
            cmd->angleturn, cmd->buttons);
    }

    // Update last_action for next tic
    int buttons = cmd->buttons;
    last_action[0] = discretize_fwd(cmd->forwardmove);
    last_action[1] = discretize_side(cmd->sidemove);
    last_action[2] = discretize_turn(cmd->angleturn);
    last_action[3] = (float)((buttons & 1) ? 1 : 0);
    last_action[4] = (float)(((buttons >> 1) & 1) ? 1 : 0);

    tic_count++;
}

void NN_CloseLogger(void)
{
    if (logfile)
    {
        fclose(logfile);
        logfile = NULL;
        fprintf(stderr, "NN_Logger: closed after %d tics\n", tic_count);
    }
}
