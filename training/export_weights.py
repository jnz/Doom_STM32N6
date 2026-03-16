"""
export_weights.py - Export trained weights to C header
Auto-detects model architecture from checkpoint.
"""

import argparse, sys, os
import numpy as np
import torch

sys.path.insert(0, os.path.dirname(__file__))
from train_doom_bot import DoomBotMLP


def format_array(name, arr, per_line=8):
    flat = arr.flatten()
    lines = [f"static const float {name}[{len(flat)}] = {{"]
    for i in range(0, len(flat), per_line):
        chunk = flat[i:i+per_line]
        vals = ", ".join(f"{v: .8f}f" for v in chunk)
        comma = "," if i + per_line < len(flat) else ""
        lines.append(f"    {vals}{comma}")
    lines.append("};")
    return "\n".join(lines)


def export_header(checkpoint_path, output_path):
    ckpt = torch.load(checkpoint_path, map_location='cpu', weights_only=False)
    sd = ckpt['model_state_dict']

    # Auto-detect architecture
    input_size = sd['shared.0.weight'].shape[1]
    layer_keys = sorted([k for k in sd if k.startswith('shared.') and k.endswith('.weight')])

    model = DoomBotMLP(input_size=input_size)
    model.load_state_dict(sd)
    model.eval()

    scaler_mean  = ckpt['scaler_mean']
    scaler_scale = ckpt['scaler_scale']

    total_params = sum(p.numel() for p in model.parameters())
    print(f"  Input size: {input_size}")
    print(f"  Layers: {len(layer_keys)}")
    print(f"  Parameters: {total_params} ({total_params*4/1024:.1f} KB FP32)")

    with open(output_path, 'w') as f:
        f.write("// nn_weights.h - auto-generated\n")
        f.write(f"// Parameters: {total_params}\n\n")
        f.write("#ifndef NN_WEIGHTS_H\n#define NN_WEIGHTS_H\n\n")

        f.write(f"#define NN_INPUT_SIZE  {input_size}\n")
        for li, key in enumerate(layer_keys):
            f.write(f"#define NN_HIDDEN{li+1}     {sd[key].shape[0]}\n")
        f.write("#define NN_OUT_FWD      3\n")
        f.write("#define NN_OUT_SIDE     3\n")
        f.write("#define NN_OUT_TURN     5\n")
        f.write("#define NN_OUT_FIRE     2\n")
        f.write("#define NN_OUT_USE      2\n\n")

        f.write(format_array("scaler_mean", scaler_mean) + "\n\n")
        f.write(format_array("scaler_scale", scaler_scale) + "\n\n")

        for li, key in enumerate(layer_keys):
            w = sd[key].numpy()
            b = sd[key.replace('weight', 'bias')].numpy()
            f.write(f"// Layer {li+1}: {w.shape[1]} -> {w.shape[0]}\n")
            f.write(format_array(f"layer{li+1}_weight", w) + "\n\n")
            f.write(format_array(f"layer{li+1}_bias", b) + "\n\n")

        for name, key, sz in [("fwd","head_fwd",3),("side","head_side",3),
                               ("turn","head_turn",5),("fire","head_fire",2),
                               ("use","head_use",2)]:
            w = sd[f'{key}.weight'].numpy()
            b = sd[f'{key}.bias'].numpy()
            f.write(f"// Head: {name} ({w.shape[1]} -> {sz})\n")
            f.write(format_array(f"head_{name}_weight", w) + "\n\n")
            f.write(format_array(f"head_{name}_bias", b) + "\n\n")

        f.write("#endif\n")

    print(f"  Exported: {output_path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--checkpoint', default='./output/doom_bot.pth')
    parser.add_argument('--output', default='./output/nn_weights.h')
    args = parser.parse_args()
    export_header(args.checkpoint, args.output)

if __name__ == '__main__':
    main()
