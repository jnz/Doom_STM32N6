## Doom Bot Training Pipeline

Dieses Projekt trainiert ein Multi-Head MLP, um einen Doom-Bot für die
N64-Engine (STM32 port) zu steuern. Das Modell nutzt Raycasting, Spieler-Status
und Monster-Vektoren als Eingabe.

### Setup Windows

```
python -m venv venv
venv\Scripts\activate.bat
pip install -r requirements.txt
```

### Setup Linux/macOS

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Training starten

Trainingsdaten in `nn_training_*.csv` Dateien in den `/data` Ordner legen.

Das Script lädt alle CSVs, normalisiert die Daten und exportiert ein ONNX-Modell.

```bash
python train_doom_bot.py --epochs 150 --data_dir ./data --output_dir ./output
```

Outputs nach dem Training:

 * `output/doom_bot.onnx`: Das finale Modell für STM32Cube AI Studio
 * `output/doom_bot.pth`: PyTorch Checkpoint inkl. Scaler-Parametern und History.
 * `output/training_curves.png`: Visualisierung von Loss und Accuracy.

### Architektur & Features

Das Modell ist ein Multi-Head MLP (35 Inputs -> 128 -> 64 -> 32 -> Heads).

#### Eingabe (35 Features)

```
    Index   Feature     Beschreibung
    0       angle       Blickrichtung [0, 1]
    1-4     player      Health, MomX, MomY, Weapon
    5-20    rays        16 Abstands-Strahlen (0..2048)
    21-29   monsters    3 Monster mit je [dx, dy, present]
    30-34   last_act    Feedback der letzten Aktion (Fwd, Side, Turn, Fire, Use)
```


| Index | Gruppe | Feature | Einheit / Wertebereich | Beschreibung |
|:---:|:---|:---|:---|:---|
| **0** | Player | `angle` | `0.0` bis `1.0` | Blickrichtung (0 bis 2 pi) |
| **1** | Player | `health` | `0` bis `200` | Aktuelle Trefferpunkte |
| **2** | Player | `momx` | `~ -30.0` bis `30.0` | Geschwindigkeit in X (Map-Units pro Tic) |
| **3** | Player | `momy` | `~ -30.0` bis `30.0` | Geschwindigkeit in Y (Map-Units pro Tic) |
| **4** | Player | `weapon` | `0` bis `8` | ID der gewählten Waffe (ReadyWeapon) |
| **5-20** | Rays | `ray0..15` | `0.0` bis `2048.0` | 16 Umgebungs-Strahlen (Map-Units) |
| **21** | Monster 0 | `m0_dx` | `~ -2048` bis `2048` | Relative X-Distanz (Positiv = Vorne) |
| **22** | Monster 0 | `m0_dy` | `~ -2048` bis `2048` | Relative Y-Distanz (Positiv = Rechts) |
| **23** | Monster 0 | `m0_present` | `0.0` oder `1.0` | 1.0 wenn Monster-Slot aktiv |
| **24** | Monster 1 | `m1_dx` | `~ -2048` bis `2048` | Relative X-Distanz |
| **25** | Monster 1 | `m1_dy` | `~ -2048` bis `2048` | Relative Y-Distanz |
| **26** | Monster 1 | `m1_present` | `0.0` oder `1.0` | 1.0 wenn Monster-Slot aktiv |
| **27** | Monster 2 | `m2_dx` | `~ -2048` bis `2048` | Relative X-Distanz |
| **28** | Monster 2 | `m2_dy` | `~ -2048` bis `2048` | Relative Y-Distanz |
| **29** | Monster 2 | `m2_present` | `0.0` oder `1.0` | 1.0 wenn Monster-Slot aktiv |
| **30** | Last Act | `last_fwd` | `0, 1, 2` | 0: Zurück, 1: Stop, 2: Vor |
| **31** | Last Act | `last_side` | `0, 1, 2` | 0: Rechts, 1: Keine, 2: Links |
| **32** | Last Act | `last_turn` | `0, 1, 2, 3, 4` | 0: Scharf R, 1: R, 2: Mittel, 3: L, 4: Scharf L |
| **33** | Last Act | `last_fire` | `0.0` oder `1.0` | 1.0 wenn im letzten Tic gefeuert wurde |
| **34** | Last Act | `last_use` | `0.0` oder `1.0` | 1.0 wenn im letzten Tic "Use" aktiv war |

*Wichtig:* Alle Werte werden im Python-Preprocessing zusätzlich skaliert, bevor sie in das Netz fließen.
*Hinweis:* Die X/Y Werte sind transformiert in das Koordinatensystem des Spielers.

### Ray Casting

16 Abstandsmessungen werden für die Rays (Index 5-20) generiert. Ausgehend von der Spielerblickrichtung, gegen den Uhrzeigersinn. Siehe Video:

<video src="doc/bsp_raycast.mp4" width="100%" controls>
  Video Tag not supported
</video>

#### Ausgabe

Typ: Multi-Head MLP
Layer: 35 -> 128 -> 64 -> 32 -> [Köpfe]
Köpfe (Heads): Forward (3), Side (3), Turn (5), Fire (2), Use (2)

 * *Forward*: 3 Klassen (Zurück, Stop, Vorwärts)
 * *Side*: 3 Klassen (Rechts, None, Links)
 * *Turn*: 5 Klassen (Scharf Links bis Scharf Rechts)
 * *Buttons*: Binär für Fire und Use

#### Beispiel-Ausgang (Modell-Output)

Das Modell gibt für jeden "Head" Logits aus. Der höchste Wert (Argmax) gewinnt.

Head: fwd (Forward/Backward)

    Logits: [-2.1, 0.5, 4.8]

    Ergebnis: Index 2 (Vorwärts) -> Der Bot läuft weiter.

Head side (Seitwärtsbewegung)

    Logits: [0.1, 3.2, -1.5]

    Ergebnis: Index 1 (None) -> Der Bot hält die Spur und tänzelt nicht seitlich.

Head: turn (Rotation)

    Logits: [-1.2, -0.5, 0.1, 3.5, 0.2]

    Ergebnis: Index 3 (Slight Right) -> Der Bot dreht sich zum Monster.

Head: fire (Schießen)

    Logits: [-3.5, 4.2]

    Ergebnis: Index 1 (Ja) -> Der Bot feuert.

Head: use (Tür öffnen)

    Logits: [5.2, -4.1]

    Ergebnis: Index = (Nein) -> Bot versucht nicht eine Tür zu öffnen.

Der Bot erhält bei jedem "Tic" (35-mal pro Sekunde) die Ausgaben.
Die Output-Heads liefern Indizes (z.B. 0, 1, 2). Diese werden über statische
Look-Up-Tabellen auf die internen Einheiten der Doom-Engine abgebildet:

    Aktion              Klassen-Indizes     Mapping auf Doom-Einheiten
    ------              ---------------     --------------------------
    Vorwärts (fwd)      0, 1, 2             {-25, 0, 25} (Laufen/Rennen)
    Seitwärts (side)    0, 1, 2             {-20, 0, 20} (Strafing)
    Drehen (turn)       0, 1, 2, 3, 4       {-512, -192, 0, 192, 512} (Präzise bis Scharf)
    Feuern / Benutzen   0, 1                `BT_ATTACK` / `BT_USE` (Bits setzen)


### C-Header Export (für direkten C-Code)

```bash
python export_weights.py --checkpoint ./output/doom_bot.pth --output ./output/nn_weights.h
```

