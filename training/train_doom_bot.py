"""
Doom Bot Training Pipeline
==========================
35 features: 5 player + 16 rays + 9 monsters (dx,dy,present) + 5 last_action
"""

import argparse, glob, os, sys
import numpy as np
import pandas as pd
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split
from sklearn.preprocessing import StandardScaler
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ============================================================
# Configuration
# ============================================================

PLAYER_COLS  = ['angle', 'health', 'momx', 'momy', 'weapon']
RAY_COLS     = [f'ray{i}' for i in range(16)]
MONSTER_COLS = []
for i in range(3):
    MONSTER_COLS += [f'm{i}_dx', f'm{i}_dy', f'm{i}_present']
ACTION_COLS  = ['last_fwd', 'last_side', 'last_turn', 'last_fire', 'last_use']

INPUT_COLS = PLAYER_COLS + RAY_COLS + MONSTER_COLS + ACTION_COLS  # 35

LABEL_COLS = ['cmd_fwd', 'cmd_side', 'cmd_turn', 'cmd_buttons']

N_FWD_CLASSES  = 3
N_SIDE_CLASSES = 3
N_TURN_CLASSES = 5
N_BUTTON_FIRE  = 2
N_BUTTON_USE   = 2

# ============================================================
# Data
# ============================================================

def load_csvs(data_dir):
    pattern = os.path.join(data_dir, '*.csv')
    files = sorted(glob.glob(pattern))
    if not files:
        print(f"ERROR: No CSV files found in {data_dir}")
        sys.exit(1)
    dfs = []
    for f in files:
        df = pd.read_csv(f)
        print(f"  Loaded {f}: {len(df)} rows")
        dfs.append(df)
    combined = pd.concat(dfs, ignore_index=True)
    print(f"  Total: {len(combined)} rows")
    return combined


def discretize_labels(df):
    labels = {}
    labels['fwd_class']  = np.clip(np.digitize(df['cmd_fwd'].values, [-10, 10]), 0, 2)
    labels['side_class'] = np.clip(np.digitize(df['cmd_side'].values, [-10, 10]), 0, 2)
    labels['turn_class'] = np.clip(np.digitize(df['cmd_turn'].values, [-384, -64, 64, 384]), 0, 4)
    buttons = pd.to_numeric(df['cmd_buttons'], errors='coerce').fillna(0).astype(int).values
    labels['fire'] = (buttons & 1).astype(int)
    labels['use']  = ((buttons >> 1) & 1).astype(int)
    return labels


def preprocess(df):
    # Drop rows with NaN in any column
    before = len(df)
    df = df.dropna()
    if len(df) < before:
        print(f"  Dropped {before - len(df)} rows with NaN")

    X = df[INPUT_COLS].values.astype(np.float32)

    # Replace any remaining inf/-inf
    X = np.nan_to_num(X, nan=0.0, posinf=2048.0, neginf=-2048.0)

    # Player [0..4]
    # X[:,0] angle already [0,1)
    X[:, 1] /= 200.0    # health
    X[:, 2] /= 30.0     # momx
    X[:, 3] /= 30.0     # momy
    X[:, 4] /= 8.0      # weapon

    # Rays [5..20]
    X[:, 5:21] /= 2048.0

    # Monsters [21..29]: groups of 3 (dx, dy, present)
    for i in range(3):
        base = 21 + i * 3
        X[:, base]     /= 2048.0   # dx
        X[:, base + 1] /= 2048.0   # dy
        # present is already 0/1

    # Last action [30..34]
    X[:, 30] /= 2.0   # fwd [0,1,2]
    X[:, 31] /= 2.0   # side [0,1,2]
    X[:, 32] /= 4.0   # turn [0..4]
    # fire, use already 0/1

    scaler = StandardScaler()
    X = scaler.fit_transform(X).astype(np.float32)

    labels = discretize_labels(df)
    return X, labels, scaler


# ============================================================
# Dataset
# ============================================================

class DoomDataset(Dataset):
    def __init__(self, X, labels):
        self.X     = torch.from_numpy(X)
        self.fwd   = torch.from_numpy(labels['fwd_class']).long()
        self.side  = torch.from_numpy(labels['side_class']).long()
        self.turn  = torch.from_numpy(labels['turn_class']).long()
        self.fire  = torch.from_numpy(labels['fire']).long()
        self.use   = torch.from_numpy(labels['use']).long()

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        return (self.X[idx], self.fwd[idx], self.side[idx],
                self.turn[idx], self.fire[idx], self.use[idx])


# ============================================================
# Model
# ============================================================

class DoomBotMLP(nn.Module):
    def __init__(self, input_size, dropout=0.2):
        super().__init__()
        self.shared = nn.Sequential(
            nn.Linear(input_size, 128),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.Linear(64, 32),
            nn.ReLU(),
        )
        self.head_fwd  = nn.Linear(32, N_FWD_CLASSES)
        self.head_side = nn.Linear(32, N_SIDE_CLASSES)
        self.head_turn = nn.Linear(32, N_TURN_CLASSES)
        self.head_fire = nn.Linear(32, N_BUTTON_FIRE)
        self.head_use  = nn.Linear(32, N_BUTTON_USE)

    def forward(self, x):
        h = self.shared(x)
        return (self.head_fwd(h), self.head_side(h), self.head_turn(h),
                self.head_fire(h), self.head_use(h))


# ============================================================
# Training
# ============================================================

def compute_class_weights(labels, key, n_classes):
    counts = np.bincount(labels[key], minlength=n_classes).astype(np.float32)
    counts = np.maximum(counts, 1.0)
    weights = 1.0 / counts
    weights /= weights.sum()
    weights *= n_classes
    return torch.from_numpy(weights)


def train_model(X, labels, epochs=200, batch_size=128, lr=1e-3, val_split=0.15):
    dataset = DoomDataset(X, labels)
    val_size = int(len(dataset) * val_split)
    train_size = len(dataset) - val_size
    train_ds, val_ds = random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True)
    val_loader   = DataLoader(val_ds, batch_size=batch_size, shuffle=False)

    model = DoomBotMLP(input_size=X.shape[1])
    n_params = sum(p.numel() for p in model.parameters())
    print(f"\n  Model parameters: {n_params}")
    print(f"  Estimated INT8 size: {n_params / 1024:.1f} KB\n")

    w_fwd  = compute_class_weights(labels, 'fwd_class', N_FWD_CLASSES)
    w_side = compute_class_weights(labels, 'side_class', N_SIDE_CLASSES)
    w_turn = compute_class_weights(labels, 'turn_class', N_TURN_CLASSES)
    w_fire = compute_class_weights(labels, 'fire', N_BUTTON_FIRE)
    w_use  = compute_class_weights(labels, 'use', N_BUTTON_USE)

    loss_fwd  = nn.CrossEntropyLoss(weight=w_fwd)
    loss_side = nn.CrossEntropyLoss(weight=w_side)
    loss_turn = nn.CrossEntropyLoss(weight=w_turn)
    loss_fire = nn.CrossEntropyLoss(weight=w_fire)
    loss_use  = nn.CrossEntropyLoss(weight=w_use)

    optimizer = optim.Adam(model.parameters(), lr=lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)

    history = {'train_loss': [], 'val_loss': [],
               'val_acc_fwd': [], 'val_acc_side': [],
               'val_acc_turn': [], 'val_acc_fire': [], 'val_acc_use': []}
    best_val_loss = float('inf')
    best_state = None

    for epoch in range(epochs):
        model.train()
        train_loss_sum, train_count = 0.0, 0
        for batch in train_loader:
            x, y_fwd, y_side, y_turn, y_fire, y_use = batch
            optimizer.zero_grad()
            p_fwd, p_side, p_turn, p_fire, p_use = model(x)
            loss = (loss_fwd(p_fwd, y_fwd) + loss_side(p_side, y_side) +
                    loss_turn(p_turn, y_turn) + loss_fire(p_fire, y_fire) +
                    loss_use(p_use, y_use))
            loss.backward()
            optimizer.step()
            train_loss_sum += loss.item() * len(x)
            train_count += len(x)

        avg_train = train_loss_sum / train_count

        model.eval()
        val_loss_sum, val_count = 0.0, 0
        correct = {'fwd': 0, 'side': 0, 'turn': 0, 'fire': 0, 'use': 0}
        with torch.no_grad():
            for batch in val_loader:
                x, y_fwd, y_side, y_turn, y_fire, y_use = batch
                p_fwd, p_side, p_turn, p_fire, p_use = model(x)
                loss = (loss_fwd(p_fwd, y_fwd) + loss_side(p_side, y_side) +
                        loss_turn(p_turn, y_turn) + loss_fire(p_fire, y_fire) +
                        loss_use(p_use, y_use))
                val_loss_sum += loss.item() * len(x)
                val_count += len(x)
                correct['fwd']  += (p_fwd.argmax(1) == y_fwd).sum().item()
                correct['side'] += (p_side.argmax(1) == y_side).sum().item()
                correct['turn'] += (p_turn.argmax(1) == y_turn).sum().item()
                correct['fire'] += (p_fire.argmax(1) == y_fire).sum().item()
                correct['use']  += (p_use.argmax(1) == y_use).sum().item()

        avg_val = val_loss_sum / val_count
        accs = {k: v / val_count for k, v in correct.items()}
        scheduler.step(avg_val)

        if avg_val < best_val_loss:
            best_val_loss = avg_val
            best_state = {k: v.clone() for k, v in model.state_dict().items()}

        history['train_loss'].append(avg_train)
        history['val_loss'].append(avg_val)
        for k in ['fwd', 'side', 'turn', 'fire', 'use']:
            history[f'val_acc_{k}'].append(accs[k])

        if (epoch + 1) % 10 == 0 or epoch == 0:
            print(f"  Epoch {epoch+1:3d}/{epochs} | "
                  f"Train: {avg_train:.4f} | Val: {avg_val:.4f} | "
                  f"fwd:{accs['fwd']:.2f} side:{accs['side']:.2f} "
                  f"turn:{accs['turn']:.2f} fire:{accs['fire']:.2f} use:{accs['use']:.2f}")

    if best_state:
        model.load_state_dict(best_state)
    return model, history


# ============================================================
# Export & Visualization
# ============================================================

def export_onnx(model, input_size, output_path):
    model.eval()
    dummy = torch.randn(1, input_size, dtype=torch.float32)
    torch.onnx.export(model, dummy, output_path,
                      input_names=['gamestate'],
                      output_names=['fwd', 'side', 'turn', 'fire', 'use'],
                      opset_version=18, do_constant_folding=True)
    size = os.path.getsize(output_path)
    print(f"  ONNX: {output_path} ({size/1024:.1f} KB)")


def plot_training(history, output_dir):
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    axes[0].plot(history['train_loss'], label='Train')
    axes[0].plot(history['val_loss'], label='Validation')
    axes[0].set_xlabel('Epoch'); axes[0].set_ylabel('Loss')
    axes[0].set_title('Loss'); axes[0].legend(); axes[0].grid(True, alpha=0.3)

    for k in ['fwd', 'side', 'turn', 'fire', 'use']:
        axes[1].plot(history[f'val_acc_{k}'], label=k)
    axes[1].set_xlabel('Epoch'); axes[1].set_ylabel('Accuracy')
    axes[1].set_title('Validation Accuracy'); axes[1].legend()
    axes[1].grid(True, alpha=0.3); axes[1].set_ylim(0, 1)

    plt.tight_layout()
    path = os.path.join(output_dir, 'training_curves.png')
    plt.savefig(path, dpi=150); plt.close()
    print(f"  Curves: {path}")


def print_label_distribution(labels):
    print("\n  Label distributions:")
    print(f"    Forward:  {np.bincount(labels['fwd_class'], minlength=3)}"
          f"  (backward / stop / forward)")
    print(f"    Side:     {np.bincount(labels['side_class'], minlength=3)}"
          f"  (right / none / left)")
    print(f"    Turn:     {np.bincount(labels['turn_class'], minlength=5)}"
          f"  (hard_R / slight_R / straight / slight_L / hard_L)")
    print(f"    Fire:     {np.bincount(labels['fire'], minlength=2)}"
          f"  (no / yes)")
    print(f"    Use:      {np.bincount(labels['use'], minlength=2)}"
          f"  (no / yes)")


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data_dir', default='./data')
    parser.add_argument('--output_dir', default='./output')
    parser.add_argument('--epochs', type=int, default=200)
    parser.add_argument('--batch_size', type=int, default=128)
    parser.add_argument('--lr', type=float, default=1e-3)
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    print("\n[1/5] Loading data...")
    df = load_csvs(args.data_dir)

    print("\n[2/5] Preprocessing...")
    X, labels, scaler = preprocess(df)
    print(f"  Input shape: {X.shape}")
    print_label_distribution(labels)

    print("\n[3/5] Training...")
    model, history = train_model(X, labels, epochs=args.epochs,
                                 batch_size=args.batch_size, lr=args.lr)

    print("\n[4/5] Exporting...")
    onnx_path = os.path.join(args.output_dir, 'doom_bot.onnx')
    export_onnx(model, X.shape[1], onnx_path)

    print("\n[5/5] Saving artifacts...")
    plot_training(history, args.output_dir)
    torch.save({'model_state_dict': model.state_dict(),
                'scaler_mean': scaler.mean_, 'scaler_scale': scaler.scale_,
                'history': history}, os.path.join(args.output_dir, 'doom_bot.pth'))

    print("  Done!\n")

if __name__ == '__main__':
    main()
