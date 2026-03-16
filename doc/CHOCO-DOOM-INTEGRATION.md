Integration Steps
-----------------

Make sure files in `src/doom/neuralnet` share the same files with
`Doom_STM32N6570_DK/STM32CubeIDE/AppS/Src/NeuralNet`

    nn_common.c + nn_common.h (Shared stuff)
    nn_infer.c + nn_infer.h (Inference-Engine)
    nn_player.c + nn_player.h (Doom-Integration)
    nn_weights.h (generiert von export_weights.py)

The logger is not supposed to be in the STM32 Embedded build.

Change `src/doom/g_game.c`:

```
    #include "nn_player.h"
```

A global variable:

    static boolean nn_bot_active = false;

In `G_BuildTiccmd()` – right at the beginning:

```
void G_BuildTiccmd(ticcmd_t *cmd)
{
    if (nn_bot_active)
    {
        NN_PlayerBuildTicCmd(&players[consoleplayer], cmd);
        return;
    }

}
```

Initialize in `G_DoLoadLevel()`:

```
int p = M_CheckParm("-nnbot");
if (p)
{
    if (NN_PlayerInit())
        nn_bot_active = true;
}
```

Run the bot on E1M5:

    Doom.exe -iwad doom1.wad -nnbot -skill 2 -warp 1 5

Play E1M5 and record .csv file(s):

             -iwad doom1.wad -skill 1 -warp 1 5

Record a demo file:

             -iwad doom1.wad -skill 1 -warp 1 5 -record demofile

