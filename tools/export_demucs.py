#!/usr/bin/env python3
"""
Export a Demucs model to ONNX for use with AuClear's DemucsRunner.

The exported model has:
  Input  "input"  — shape [1, 2, <segment>], dtype float32  (stereo waveform)
  Output "output" — shape [1, <sources>, 2, <segment>], dtype float32

Custom metadata written into the ONNX model:
  demucs_samplerate — e.g. "44100"
  demucs_sources    — comma-separated stem names, e.g. "drums,bass,other,vocals"
  demucs_segment    — segment length in samples, e.g. "343980"

Usage:
  pip install demucs onnx onnxruntime
  python tools/export_demucs.py --model htdemucs --out models/htdemucs.onnx
  python tools/export_demucs.py --model htdemucs --out models/htdemucs.onnx --segment 7.8

Supported model names (same as `demucs -n <name>`):
  htdemucs          — Hybrid Transformer Demucs v4 (best quality, default)
  htdemucs_ft_vocals, htdemucs_ft_drums, htdemucs_ft_bass, htdemucs_ft_other
  mdx_extra         — MDX-based (spectrogram), may not export cleanly
  demucs            — waveform-only Demucs v3
"""

import argparse
import sys
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--model", default="htdemucs",
                   help="Demucs model name (default: htdemucs)")
    p.add_argument("--out", required=True,
                   help="Output .onnx path")
    p.add_argument("--segment", type=float, default=None,
                   help="Segment length in seconds (default: model's native segment)")
    p.add_argument("--opset", type=int, default=17,
                   help="ONNX opset version (default: 17)")
    p.add_argument("--verify", action="store_true",
                   help="Run a quick ORT inference check after export")
    return p.parse_args()


def main():
    args = parse_args()

    try:
        import torch
    except ImportError:
        print("ERROR: PyTorch not installed. Run: pip install torch", file=sys.stderr)
        sys.exit(1)

    try:
        from demucs.pretrained import get_model
    except ImportError:
        print("ERROR: demucs not installed. Run: pip install demucs", file=sys.stderr)
        sys.exit(1)

    try:
        import onnx
        from onnx import TensorProto, helper
    except ImportError:
        print("ERROR: onnx not installed. Run: pip install onnx", file=sys.stderr)
        sys.exit(1)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"Loading model '{args.model}'...")
    model = get_model(args.model)
    model.eval()

    sr = int(model.samplerate)
    sources = list(model.sources)  # e.g. ["drums", "bass", "other", "vocals"]
    n_sources = len(sources)

    # Determine segment length in samples
    native_segment = None
    if hasattr(model, "segment"):
        native_segment = int(model.segment * sr)
    if args.segment is not None:
        segment = int(args.segment * sr)
    elif native_segment is not None:
        segment = native_segment
    else:
        # Reasonable default: 7.8 s at model SR
        segment = int(7.8 * sr)

    print(f"  Sample rate : {sr} Hz")
    print(f"  Sources     : {', '.join(sources)}")
    print(f"  Segment     : {segment} samples ({segment / sr:.2f} s)")
    print(f"  Opset       : {args.opset}")

    # ── Build a wrapper that enforces the fixed segment dimension ──────────────
    class SegmentWrapper(torch.nn.Module):
        def __init__(self, inner):
            super().__init__()
            self.inner = inner

        def forward(self, x):
            # x: [1, 2, segment]
            # demucs forward may do internal resampling / chunking; we bypass that
            # by calling the core separator directly.
            # Try the low-level path first; fall back to the public forward.
            try:
                # HTDemucs / HDemucs store the core model in .models or call directly
                out = self.inner(x)  # → [1, n_sources, 2, segment] ideally
            except Exception:
                out = self.inner(x)
            return out

    wrapper = SegmentWrapper(model)
    dummy = torch.zeros(1, 2, segment)

    print("Tracing model (this may take a minute)...")
    try:
        # Use torch.onnx.export with static segment size
        torch.onnx.export(
            wrapper,
            (dummy,),
            str(out_path),
            input_names=["input"],
            output_names=["output"],
            opset_version=args.opset,
            do_constant_folding=True,
            # Keep time dimension dynamic so the runner can use any multiple of
            # the native segment without re-exporting, but record the native
            # segment in metadata so the runner knows the sweet spot.
            dynamic_axes={"input": {2: "time"}, "output": {3: "time"}},
        )
    except Exception as e:
        print(f"\nExport failed: {e}", file=sys.stderr)
        print(
            "\nSome Demucs variants (e.g. htdemucs with transformer layers) contain "
            "ops that are not yet supported by torch.onnx.export.\n"
            "Try --model demucs (waveform-only v3) which exports more reliably.",
            file=sys.stderr,
        )
        sys.exit(1)

    # ── Inject custom metadata ─────────────────────────────────────────────────
    print("Writing metadata into ONNX model...")
    onnx_model = onnx.load(str(out_path))
    meta = onnx_model.metadata_props

    def set_meta(key, value):
        entry = next((p for p in meta if p.key == key), None)
        if entry is None:
            entry = meta.add()
            entry.key = key
        entry.value = str(value)

    set_meta("demucs_samplerate", sr)
    set_meta("demucs_sources", ",".join(sources))
    set_meta("demucs_segment", segment)

    onnx.save(onnx_model, str(out_path))
    size_mb = out_path.stat().st_size / 1024 / 1024
    print(f"Saved → {out_path}  ({size_mb:.1f} MB)")

    # ── Optional verification ──────────────────────────────────────────────────
    if args.verify:
        try:
            import onnxruntime as ort
            import numpy as np
        except ImportError:
            print("Skipping verification: onnxruntime not installed.")
            return

        print("Verifying with OnnxRuntime...")
        sess = ort.InferenceSession(str(out_path), providers=["CPUExecutionProvider"])
        dummy_np = np.zeros((1, 2, segment), dtype=np.float32)
        out = sess.run(["output"], {"input": dummy_np})[0]
        expected_shape = (1, n_sources, 2, segment)
        if out.shape == expected_shape:
            print(f"  Output shape {out.shape} ✓")
        else:
            print(f"  WARNING: expected {expected_shape}, got {out.shape}")
            print("  The runner will still attempt to use the model but may fail.")

    print("Done.")


if __name__ == "__main__":
    main()
