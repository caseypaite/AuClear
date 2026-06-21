#!/usr/bin/env python3
"""
Export a PyTorch time-domain audio denoising model to ONNX.

This script demonstrates how to define a PyTorch neural network for real-time
audio denoising, shape its inputs/outputs to match AuClear's time-domain contract,
export it to ONNX, and generate the required config.ini file.

Usage:
    pip install torch onnx
    python tools/export_denoise_model.py --out models/my_unet_denoiser.onnx
"""

import os
import argparse
import torch
import torch.nn as nn


class AudioUNet(nn.Module):
    """
    A lightweight Conv1D U-Net for real-time, time-domain audio denoising.
    Processes audio frame-by-frame (e.g. 480 samples = 10 ms at 48 kHz).
    
    Inputs:
        x (Tensor): [1, frame_size] - Raw time-domain float32 samples.
    Outputs:
        y (Tensor): [1, frame_size] - Cleaned time-domain float32 samples.
    """
    def __init__(self):
        super().__init__()
        
        # Encoder layers (downsampling time resolution)
        self.enc1 = nn.Conv1d(1, 16, kernel_size=15, stride=2, padding=7)
        self.enc2 = nn.Conv1d(16, 32, kernel_size=15, stride=2, padding=7)
        self.enc3 = nn.Conv1d(32, 64, kernel_size=15, stride=2, padding=7)
        
        # Decoder layers (upsampling time resolution back to original)
        self.dec3 = nn.ConvTranspose1d(64, 32, kernel_size=16, stride=2, padding=7)
        self.dec2 = nn.ConvTranspose1d(32, 16, kernel_size=16, stride=2, padding=7)
        self.dec1 = nn.ConvTranspose1d(16, 1, kernel_size=16, stride=2, padding=7)
        
        self.relu = nn.ReLU()
        self.tanh = nn.Tanh()

    def forward(self, x):
        # 1. Reshape from 2D [batch, time] to 3D [batch, channels, time] for Conv1D
        x_3d = x.unsqueeze(1)
        
        # 2. Encoder pass
        e1 = self.relu(self.enc1(x_3d))
        e2 = self.relu(self.enc2(e1))
        e3 = self.relu(self.enc3(e2))
        
        # 3. Decoder pass with skip connections
        d3 = self.relu(self.dec3(e3))
        d2 = self.relu(self.dec2(d3 + e2))
        d1 = self.tanh(self.dec1(d2 + e1))
        
        # 4. Reshape back to 2D [batch, time]
        out = d1.squeeze(1)
        return out


def export_model(out_path: str, sample_rate: int, frame_size: int):
    # Instantiate the model
    model = AudioUNet()
    model.eval()
    
    # Create dummy input matching the shape expected by AuClear [1, frame_size]
    dummy_input = torch.zeros(1, frame_size, dtype=torch.float32)
    
    # Ensure parent directory exists
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    
    # Export to ONNX
    torch.onnx.export(
        model,
        dummy_input,
        out_path,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch"},
            "output": {0: "batch"}
        },
        opset_version=17
    )
    
    print(f"Exported ONNX model to: {out_path}")
    
    # Generate accompanying config.ini file
    config_path = os.path.splitext(out_path)[0] + ".config.ini"
    with open(config_path, "w") as f:
        f.write("[df]\n")
        f.write(f"sr = {sample_rate}\n")
        f.write(f"hop_size = {frame_size}\n")
        f.write("fft_size = 960\n")
        
    print(f"Generated accompanying configuration file: {config_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Export a time-domain denoise model to ONNX for AuClear.")
    parser.add_argument("--out", type=str, default=None,
                        help="Output ONNX file path (default: models/unet_denoiser.onnx)")
    parser.add_argument("--sr", type=int, default=48000,
                        help="Model target sample rate (default: 48000)")
    parser.add_argument("--frame-size", type=int, default=480,
                        help="Model block processing size (default: 480 samples = 10ms at 48kHz)")
    args = parser.parse_args()
    
    default_out = os.path.join(os.path.dirname(__file__), "..", "models", "unet_denoiser.onnx")
    out_path = args.out or default_out
    
    export_model(out_path, args.sr, args.frame_size)
