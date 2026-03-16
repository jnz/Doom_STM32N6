# Doom Neural Network Bot on STM32N6

![Doom on STM32N6](./doc/doom_stm32n6.gif)

This project implements an end-to-end pipeline for training a neural network to
play Doom, and deploying it on an STM32N6570-DK microcontroller. It covers data
collection through human gameplay, model training in Python, and real-time
inference in C on embedded hardware.

## Repository Structure

```
.
├── chocolate-doom/         # Modified Chocolate Doom (data recording & NN bot playback)
├── training/               # Python training pipeline (.CSV PyTorch to ONNX / C header)
└── Doom_STM32N6570_DK/     # STM32CubeIDE project for on-device inference
```

## Overview

| Component | Purpose | Details |
|---|---|---|
| **chocolate-doom/** | Play Doom on Linux/WSL2, record training data as CSV, or run the trained bot | [chocolate-doom/README.md](chocolate-doom/README.md) |
| **training/** | Process CSV recordings, train a multi-head MLP, export to ONNX and C header | [training/README.md](training/README.md) |
| **Doom_STM32N6570_DK/** | STM32CubeIDE project that runs Doom with the trained model on the N6 board | [Doom_STM32N6570_DK/README.md](Doom_STM32N6570_DK/README.md) |

## Workflow

```
 1. Play Doom        2. Train Model         3. Deploy to MCU
┌──────────────┐   ┌───────────────────┐   ┌───────────────────────┐
│ chocolate-doom│──▶│ train_doom_bot.py │──▶│ STM32CubeIDE Project  │
│              │   │                   │   │                       │
│ Record CSVs  │   │ CSV → MLP → ONNX  │   │ nn_weights.h → C code │
│ from gameplay│   │       → C header  │   │ on STM32N6570-DK      │
└──────────────┘   └───────────────────┘   └───────────────────────┘
```

## Quick Start

### 1. Record Training Data

Play the game and Chocolate Doom will log your inputs to CSV files:

```bash
cd chocolate-doom
# Build according to chocolate-doom/README.md, then e.g. train on E1M5:
./src/chocolate-doom -iwad ../Doom_STM32N6570_DK/wad/doom1.wad -skill 1 -warp 1 5 -window -3 -nosfx -nomusic
```

### 2. Train the Model

See `training/README.md` for details.

```bash
cd training
python3 -m venv venv
source venv/bin/activate          # Windows: venv\Scripts\activate.bat
pip install -r requirements.txt

# Place CSV files in ./data/, then:
python train_doom_bot.py --epochs 150 --data_dir ./data --output_dir ./output
```

**Outputs:**

- `/training/output/doom_bot.onnx` – Model for STM32Cube AI Studio
- `/training/output/doom_bot.pth` – PyTorch checkpoint (includes scaler parameters and training history)
- `/training/output/training_curves.png` – Loss and accuracy plots

### 3. Export C Header

```bash
python export_weights.py --checkpoint ./output/doom_bot.pth --output ./output/nn_weights.h
```

### 4. Deploy on STM32N6570-DK

Copy `nn_weights.h` into the STM32CubeIDE project and flash the board.
See [Doom_STM32N6570_DK/README.md](Doom_STM32N6570_DK/README.md) for build and flash instructions.

## Model Architecture

### Input Features (35 total)

| Index | Group | Feature | Description |
|:---:|---|---|---|
| 0 | Player | `angle` | View direction, normalized to [0, 1] |
| 1 | Player | `health` | Hit points (0–200) |
| 2–3 | Player | `momx`, `momy` | Velocity in map units per tic |
| 4 | Player | `weapon` | Active weapon ID (0–8) |
| 5–20 | Rays | `ray0`–`ray15` | 16 distance rays (0–2048 map units), counter-clockwise from view direction |
| 21–29 | Monsters | `m0_dx/dy/present` ... `m2_dx/dy/present` | Relative position and presence flag for up to 3 nearest monsters |
| 30–34 | Last Action | `last_fwd/side/turn/fire/use` | Feedback of the previous tic's action |

All values are additionally scaled during Python preprocessing before being fed
into the network. Monster coordinates are transformed into the player's local
coordinate system.

### Output Heads

| Head | Classes | Mapping to Doom Engine |
|---|---|---|
| Forward | 3 (back, stop, forward) | {-25, 0, 25} |
| Side | 3 (right, none, left) | {-20, 0, 20} |
| Turn | 5 (sharp-R … sharp-L) | {-512, -192, 0, 192, 512} |
| Fire | 2 (no, yes) | `BT_ATTACK` bit |
| Use | 2 (no, yes) | `BT_USE` bit |

At every game tic (35 Hz), the model produces one action across all five heads. The highest logit per head determines the chosen class, which is then mapped to engine units via static lookup tables.

## Chocolate Doom Integration

The Chocolate Doom fork includes a lightweight NN integration. Five new source files are added to `src/doom/`, and one existing file (`g_game.c`) is modified:

**New files in `src/doom/`:**

- `nn_infer.c` / `nn_infer.h` – Inference engine
- `nn_player.c` / `nn_player.h` – Doom integration (reads game state, writes tic commands)
- `nn_weights.h` – Generated weight header (from `export_weights.py`)

**Changes in `g_game.c`:**

- Include `nn_player.h`
- Add a `static boolean nn_bot_active` flag
- In `G_BuildTiccmd()`: if the bot is active, call `NN_PlayerBuildTicCmd()` and return early
- In `G_DoLoadLevel()` / `G_InitNew()`: check for the `-nnbot` command-line flag and initialize the bot
- In the quit routine: call `NN_PlayerShutdown()`

**Running the bot (example on E1M5):**

```bash
./src/chocolate-doom -iwad ../path/to/doom1.wad -nnbot -skill 2 -warp 1 5
```

When launched *without* `-nnbot`, the game initializes the CSV logger instead, so every regular play session produces training data.

## Requirements

- **Chocolate Doom**: Linux or WSL2 (see [chocolate-doom/README.md](chocolate-doom/README.md))
- **Training**: Python 3.8+, PyTorch, dependencies in `training/requirements.txt`
- **Deployment**: STM32CubeIDE, STM32N6570-DK board

## License

See individual subdirectory READMEs for licensing details. Chocolate Doom is licensed under the GNU GPL v2.

