# Third-Party Licenses

Enumerates every dependency and bundled asset shipped in AuClear, with its
license and (for AI models) **weight-redistribution status**. This file is a
release gate — see [docs/06-tech-stack-build.md](docs/06-tech-stack-build.md#licensing).

## Frameworks & libraries

| Component | Version | License | Status |
|---|---|---|---|
| JUCE | 8.0.4 | GPLv3 **or** commercial | ⚠️ Commercial licence required for a closed-source release |
| Steinberg VST3 SDK | (via JUCE) | GPLv3 / Steinberg | ⚠️ Accept Steinberg terms to ship VST3 |
| LV2 headers | (via JUCE) | ISC | OK |
| ONNX Runtime | TBD | MIT | OK (added in Phase 3) |
| libebur128 | TBD | MIT | OK (added in Phase 2) |
| pffft / JUCE FFT | — | BSD / JUCE | OK |

## Bundled AI models (added Phase 3–4)

| Model | Role | Code license | ⚠️ Weight redistribution |
|---|---|---|---|
| DeepFilterNet | RT denoise / dereverb | MIT / Apache-2.0 | **Verify before bundling** |
| RNNoise | denoise fallback | BSD | OK |
| Demucs (htdemucs) | offline 4-stem | MIT (code) | **Clear weight terms before bundling** |

## Fonts

| Font | License |
|---|---|
| Inter | SIL OFL 1.1 |
| JetBrains Mono | SIL OFL 1.1 |

> Code license ≠ the right to redistribute trained model **weights** in a
> commercial product. Resolve every ⚠️ row before 1.0.
