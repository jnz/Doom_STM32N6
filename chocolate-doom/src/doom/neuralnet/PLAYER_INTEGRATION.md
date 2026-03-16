Die Integration ist überschaubar – 5 neue Dateien ins Projekt legen und eine bestehende Datei ändern:

Neue Dateien nach src/doom/ kopieren:

    nn_infer.c + nn_infer.h (Inferenz-Engine)
    nn_player.c + nn_player.h (Doom-Integration)
    nn_weights.h (generiert von export_weights.py)

Änderungen in `src/doom/g_game.c`:
Oben bei den Includes:

```
    #include "nn_player.h"
```

Eine globale Variable:

    static boolean nn_bot_active = false;

In `G_BuildTiccmd()` – ganz am Anfang der Funktion, vor allem anderen:

```
void G_BuildTiccmd(ticcmd_t *cmd)
{
    if (nn_bot_active)
    {
        NN_PlayerBuildTicCmd(&players[consoleplayer], cmd);
        return;
    }

    // ... der ganze bestehende Code bleibt ...
}
```

In `G_DoLoadLevel()` oder `G_InitNew()` – irgendwo nachdem das Level initialisiert ist:

```
int p = M_CheckParm("-nnbot");
if (p)
{
    if (NN_PlayerInit())
        nn_bot_active = true;
}
```

In der Quit-Routine (such nach `I_Quit` oder `D_QuitGame`):

```
    NN_PlayerShutdown();
```

Neu bauen und starten mit (E1M5):

    Doom.exe -iwad doom1.wad -nnbot -skill 2 -warp 1 5

Selbst spielen für Vorlagen (E1M5):

             -iwad doom1.wad -skill 1 -warp 1 5

Kann man auch als Demo aufnehmen:

             -iwad doom1.wad -skill 1 -warp 1 5 -record jandemo

