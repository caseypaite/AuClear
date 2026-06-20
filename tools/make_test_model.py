#!/usr/bin/env python3
"""
Generate a minimal ONNX model compatible with AuClear's OnnxSession interface.

The model passes audio through unchanged (identity), useful for verifying that
the AIEngine pipeline (ReBlocker, resampler, dry delay) introduces exactly the
expected latency and that the DenoisePanel loads and reports "Model ready".

Usage:
    pip install onnx numpy
    python tools/make_test_model.py [--frame-size 480] [--out models/identity.onnx]

Expected model contract (OnnxSession.h):
    input  "input"  — shape [1, frameSize], dtype float32
    output "output" — shape [1, frameSize], dtype float32
"""

import argparse
import numpy as np
import onnx
from onnx import helper, TensorProto, numpy_helper


def make_identity_model(frame_size: int, path: str) -> None:
    input_tensor = helper.make_tensor_value_info("input",  TensorProto.FLOAT, [1, frame_size])
    output_tensor = helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, frame_size])

    identity_node = helper.make_node("Identity", inputs=["input"], outputs=["output"])

    graph = helper.make_graph([identity_node], "identity_denoiser", [input_tensor], [output_tensor])
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)])
    model.ir_version = 8

    onnx.checker.check_model(model)
    onnx.save(model, path)
    print(f"Saved identity model ({frame_size} samples/frame) → {path}")


def make_noise_gate_model(frame_size: int, threshold: float, path: str) -> None:
    """
    Simple spectral energy gate: attenuates frames whose RMS is below threshold.
    Implemented as: output = input * max(0, 1 - threshold / max(rms, eps))
    """
    input_t  = helper.make_tensor_value_info("input",  TensorProto.FLOAT, [1, frame_size])
    output_t = helper.make_tensor_value_info("output", TensorProto.FLOAT, [1, frame_size])

    thr_val  = numpy_helper.from_array(np.array([[threshold]], dtype=np.float32), name="thr")
    eps_val  = numpy_helper.from_array(np.array([[1e-9]],     dtype=np.float32), name="eps")
    one_val  = numpy_helper.from_array(np.array([[1.0]],      dtype=np.float32), name="one")
    two_val  = numpy_helper.from_array(np.array([[2.0]],      dtype=np.float32), name="two")

    # rms = sqrt(mean(input^2))
    nodes = [
        helper.make_node("Pow",      ["input", "two"],       ["sq"]),
        helper.make_node("ReduceMean", ["sq"],               ["mean_sq"], axes=[1], keepdims=1),
        helper.make_node("Sqrt",     ["mean_sq"],             ["rms"]),
        helper.make_node("Add",      ["rms", "eps"],          ["rms_eps"]),
        helper.make_node("Div",      ["thr", "rms_eps"],      ["ratio"]),
        helper.make_node("Sub",      ["one", "ratio"],        ["gate_pre"]),
        helper.make_node("Relu",     ["gate_pre"],            ["gate"]),
        helper.make_node("Mul",      ["input", "gate"],       ["output"]),
    ]

    graph = helper.make_graph(nodes, "noise_gate_denoiser",
                              [input_t], [output_t],
                              initializer=[thr_val, eps_val, one_val, two_val])
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)])
    model.ir_version = 8

    onnx.checker.check_model(model)
    onnx.save(model, path)
    print(f"Saved noise-gate model (threshold={threshold}, {frame_size} samples/frame) → {path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate a test ONNX model for AuClear.")
    parser.add_argument("--frame-size", type=int, default=480,
                        help="Samples per frame at 48 kHz (default 480 = 10 ms)")
    parser.add_argument("--type", choices=["identity", "gate"], default="identity",
                        help="Model type: identity (passthrough) or gate (simple noise gate)")
    parser.add_argument("--threshold", type=float, default=0.02,
                        help="Gate threshold (0–1 RMS, gate type only)")
    parser.add_argument("--out", type=str, default=None,
                        help="Output path (default: models/<type>.onnx)")
    args = parser.parse_args()

    import os
    out_path = args.out or os.path.join(
        os.path.dirname(__file__), "..", "models", f"{args.type}.onnx")

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)

    if args.type == "identity":
        make_identity_model(args.frame_size, out_path)
    else:
        make_noise_gate_model(args.frame_size, args.threshold, out_path)
