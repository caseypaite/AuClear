#pragma once
#include "../engine/RackModule.h"
#include <JuceHeader.h>
#include <array>
#include <vector>

// STFT overlap-add spectral processor.
//
// Each channel has an independent StftEngine that accumulates kHop input
// samples, runs a 1024-point analysis FFT, attenuates bins in the user-defined
// frequency range, and IFFT+OLAs back to the output stream.
//
// Latency: kFFTSize - kHop = 768 samples (reported to host via latencySamples()).
// Normalization: WOLA with Hann × Hann at 75% overlap gives a round-trip gain of
//   N × Σ w²(n - r·H)  = 1024 × 1.5 = 1536  (JUCE IFFT not normalized).
// normScale = 1 / 1536 is precomputed and stored per-engine.
//
// Spectrogram FIFO: after each forward FFT on channel 0, magnitude bins are pushed
// to spectroFifo for the GUI to drain at 30 Hz.

class SpectralRepairModule : public RackModule
{
  public:
    static constexpr int kOrder   = 10;
    static constexpr int kFFTSize = 1 << kOrder;   // 1024
    static constexpr int kHop     = kFFTSize / 4;  // 256  (75% overlap)
    static constexpr int kLatency = kFFTSize - kHop; // 768
    static constexpr int kNumBins = kFFTSize / 2;  // 512

    static constexpr int kFifoDepth = 64;
    using MagFrame = std::array<float, kNumBins>;

    std::atomic<float> freqLo{80.f};    // Hz — lower bound of repair band
    std::atomic<float> freqHi{8000.f};  // Hz — upper bound of repair band
    std::atomic<float> attenDb{12.f};   // dB — attenuation applied to repair band

    SpectralRepairModule ();

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset   () override;
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    int  latencySamples () const override { return kLatency; }

    ModuleType   type () const override { return ModuleType::SpectralRepair; }
    juce::String name () const override { return "Spectral Repair"; }
    void getState (juce::ValueTree& tree) const override;
    void setState (const juce::ValueTree& tree) override;

    // GUI thread: returns true if a new magnitude frame was available
    bool drainSpectro (MagFrame& out) noexcept;

  private:
    struct StftEngine
    {
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window;         // Hann, kFFTSize
        std::vector<float> fftBuf;         // 2 × kFFTSize  (complex interleaved)
        std::vector<float> inputFifo;      // kHop samples — accumulates until FFT fires
        std::vector<float> outputFifo;     // kHop normalised output samples
        std::vector<float> analysisFrame;  // kFFTSize  — sliding input history
        std::vector<float> synthesisAcc;   // 2 × kFFTSize  — OLA accumulator (ring)
        int  inputPos{0};
        int  outputPos{0};
        int  synthesisPos{0};
        float normScale{1.f};

        void prepare (double sr);
        void reset () noexcept;

        // Returns one output sample. If pushSpectro is true and spectroOut is non-null,
        // fills spectroOut with normalised magnitude bins (only when FFT fires this call).
        float processSample (float x, float attenGain, int loIdx, int hiIdx,
                             bool pushSpectro, MagFrame* spectroOut) noexcept;
    };

    std::vector<StftEngine> channels;
    double sampleRate{44100.0};

    juce::AbstractFifo spectroFifo{kFifoDepth};
    std::array<MagFrame, kFifoDepth> spectroRing{};
};
