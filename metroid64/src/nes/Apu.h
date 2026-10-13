#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace Nes {

// The 2A03's audio unit: two pulse channels, triangle, noise and DMC, the frame counter and its IRQ,
// mixed with the hardware's non-linear mixer and output filters.
class Apu {
  public:
    // DMC sample fetches read CPU memory through this.
    explicit Apu(std::function<uint8_t(uint16_t)> read) : mRead(std::move(read)) {
    }

    void Reset();
    void Write(uint16_t addr, uint8_t value);
    uint8_t ReadStatus(); // $4015
    // Advances by this many CPU cycles.
    void Clock(int cycles);
    bool Irq() const {
        return mFrameIrq || mDmcIrq;
    }

    // Output samples (signed 16-bit mono) produced so far, at SampleRate().
    std::vector<int16_t>& Samples() {
        return mSamples;
    }
    // CPU cycles per output sample. 735 samples per 60 Hz frame gives 44100 Hz.
    static constexpr double kCyclesPerSample = 29780.5 / 735.0;
    static constexpr int kSampleRate = 44100;

  private:
    struct Envelope {
        bool start = false, loop = false, constant = false;
        uint8_t period = 0, divider = 0, decay = 0;
        void Clock();
        uint8_t Volume() const {
            return constant ? period : decay;
        }
    };
    struct Pulse {
        bool enabled = false;
        uint8_t duty = 0, step = 0;
        uint16_t period = 0, timer = 0;
        uint8_t length = 0;
        Envelope env;
        bool sweepEnabled = false, sweepNegate = false, sweepReload = false;
        uint8_t sweepPeriod = 0, sweepShift = 0, sweepDivider = 0;
        bool onesComplement = false; // pulse 1's sweep negates with one's complement
        uint16_t SweepTarget() const;
        bool Muted() const;
        void ClockSweep();
        uint8_t Output() const;
    };
    struct Triangle {
        bool enabled = false, control = false, reload = false;
        uint16_t period = 0, timer = 0;
        uint8_t length = 0, linear = 0, linearReload = 0, step = 0;
    };
    struct Noise {
        bool enabled = false, mode = false;
        uint16_t period = 0, timer = 0, shift = 1;
        uint8_t length = 0;
        Envelope env;
    };
    struct Dmc {
        bool enabled = false, irqEnabled = false, loop = false;
        uint16_t period = 0, timer = 0;
        uint16_t sampleAddr = 0xC000, sampleLength = 1, addr = 0, remaining = 0;
        uint8_t output = 0, shift = 0, bits = 8, buffer = 0;
        bool bufferFull = false, silent = true;
    };

    void QuarterFrame();
    void HalfFrame();
    void StepFrameCounter();
    void StartDmc();
    void FetchDmc();
    float Mix() const;

    std::function<uint8_t(uint16_t)> mRead;
    Pulse mPulse[2];
    Triangle mTri;
    Noise mNoise;
    Dmc mDmc;
    bool mFiveStep = false, mIrqInhibit = false, mFrameIrq = false, mDmcIrq = false;
    uint32_t mFrameCycle = 0;
    bool mOddCycle = false;

    // Output: box-filtered to the sample rate, then the NES's high-pass (90 Hz, 440 Hz) and low-pass
    // (14 kHz) filters.
    double mSampleClock = 0;
    double mAccum = 0;
    int mAccumCount = 0;
    float mHp90Prev = 0, mHp90Out = 0, mHp440Prev = 0, mHp440Out = 0, mLpOut = 0;
    std::vector<int16_t> mSamples;
};

} // namespace Nes
