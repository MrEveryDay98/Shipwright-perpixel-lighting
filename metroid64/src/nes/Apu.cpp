#include "nes/Apu.h"

#include <algorithm>
#include <cmath>

namespace Nes {

namespace {

const uint8_t kLengthTable[32] = { 10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
                                   12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30 };
const uint8_t kDuty[4][8] = {
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 1, 1, 1 },
};
// NTSC periods in CPU cycles.
const uint16_t kNoisePeriod[16] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
const uint16_t kDmcPeriod[16] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54 };

// Frame counter steps, in CPU cycles since the counter was reset (NTSC).
constexpr uint32_t kStep1 = 7457, kStep2 = 14913, kStep3 = 22371, kStep4 = 29829, kStep5 = 37281;

struct MixTables {
    float pulse[31];
    float tnd[203];
    MixTables() {
        pulse[0] = 0;
        for (int i = 1; i < 31; i++) {
            pulse[i] = 95.52f / (8128.0f / i + 100.0f);
        }
        tnd[0] = 0;
        for (int i = 1; i < 203; i++) {
            tnd[i] = 163.67f / (24329.0f / i + 100.0f);
        }
    }
};
const MixTables kMix;

float OnePoleAlpha(double cutoffHz, bool highPass) {
    double dt = 1.0 / Apu::kSampleRate;
    double rc = 1.0 / (2.0 * M_PI * cutoffHz);
    return (float)(highPass ? rc / (rc + dt) : dt / (rc + dt));
}

} // namespace

void Apu::Envelope::Clock() {
    if (start) {
        start = false;
        decay = 15;
        divider = period;
    } else if (divider == 0) {
        divider = period;
        if (decay > 0) {
            decay--;
        } else if (loop) {
            decay = 15;
        }
    } else {
        divider--;
    }
}

uint16_t Apu::Pulse::SweepTarget() const {
    uint16_t change = period >> sweepShift;
    if (sweepNegate) {
        return period - change - (onesComplement ? 1 : 0);
    }
    return period + change;
}

bool Apu::Pulse::Muted() const {
    return period < 8 || (!sweepNegate && SweepTarget() > 0x7FF);
}

void Apu::Pulse::ClockSweep() {
    if (sweepDivider == 0 && sweepEnabled && sweepShift > 0 && !Muted()) {
        period = SweepTarget();
    }
    if (sweepDivider == 0 || sweepReload) {
        sweepDivider = sweepPeriod;
        sweepReload = false;
    } else {
        sweepDivider--;
    }
}

uint8_t Apu::Pulse::Output() const {
    if (length == 0 || Muted() || !kDuty[duty][step]) {
        return 0;
    }
    return env.Volume();
}

void Apu::Reset() {
    mPulse[0] = Pulse{};
    mPulse[1] = Pulse{};
    mPulse[0].onesComplement = true;
    mTri = Triangle{};
    mNoise = Noise{};
    mDmc = Dmc{};
    mDmc.period = kDmcPeriod[0];
    mNoise.period = kNoisePeriod[0];
    mFiveStep = mIrqInhibit = mFrameIrq = mDmcIrq = false;
    mFrameCycle = 0;
    mOddCycle = false;
    mSampleClock = 0;
    mAccum = 0;
    mAccumCount = 0;
    mHp90Prev = mHp90Out = mHp440Prev = mHp440Out = mLpOut = 0;
    mSamples.clear();
}

void Apu::Write(uint16_t addr, uint8_t v) {
    if (addr <= 0x4007) {
        Pulse& p = mPulse[(addr >> 2) & 1];
        switch (addr & 3) {
            case 0:
                p.duty = v >> 6;
                p.env.loop = v & 0x20;
                p.env.constant = v & 0x10;
                p.env.period = v & 0x0F;
                break;
            case 1:
                p.sweepEnabled = v & 0x80;
                p.sweepPeriod = (v >> 4) & 7;
                p.sweepNegate = v & 0x08;
                p.sweepShift = v & 7;
                p.sweepReload = true;
                break;
            case 2:
                p.period = (p.period & 0x700) | v;
                break;
            case 3:
                p.period = (p.period & 0xFF) | ((v & 7) << 8);
                if (p.enabled) {
                    p.length = kLengthTable[v >> 3];
                }
                p.step = 0;
                p.env.start = true;
                break;
        }
        return;
    }
    switch (addr) {
        case 0x4008:
            mTri.control = v & 0x80;
            mTri.linearReload = v & 0x7F;
            break;
        case 0x400A:
            mTri.period = (mTri.period & 0x700) | v;
            break;
        case 0x400B:
            mTri.period = (mTri.period & 0xFF) | ((v & 7) << 8);
            if (mTri.enabled) {
                mTri.length = kLengthTable[v >> 3];
            }
            mTri.reload = true;
            break;
        case 0x400C:
            mNoise.env.loop = v & 0x20;
            mNoise.env.constant = v & 0x10;
            mNoise.env.period = v & 0x0F;
            break;
        case 0x400E:
            mNoise.mode = v & 0x80;
            mNoise.period = kNoisePeriod[v & 0x0F];
            break;
        case 0x400F:
            if (mNoise.enabled) {
                mNoise.length = kLengthTable[v >> 3];
            }
            mNoise.env.start = true;
            break;
        case 0x4010:
            mDmc.irqEnabled = v & 0x80;
            if (!mDmc.irqEnabled) {
                mDmcIrq = false;
            }
            mDmc.loop = v & 0x40;
            mDmc.period = kDmcPeriod[v & 0x0F];
            break;
        case 0x4011:
            mDmc.output = v & 0x7F;
            break;
        case 0x4012:
            mDmc.sampleAddr = 0xC000 + v * 64;
            break;
        case 0x4013:
            mDmc.sampleLength = v * 16 + 1;
            break;
        case 0x4015:
            mPulse[0].enabled = v & 0x01;
            mPulse[1].enabled = v & 0x02;
            mTri.enabled = v & 0x04;
            mNoise.enabled = v & 0x08;
            mDmc.enabled = v & 0x10;
            for (Pulse& p : mPulse) {
                if (!p.enabled) {
                    p.length = 0;
                }
            }
            if (!mTri.enabled) {
                mTri.length = 0;
            }
            if (!mNoise.enabled) {
                mNoise.length = 0;
            }
            if (!mDmc.enabled) {
                mDmc.remaining = 0;
            } else if (mDmc.remaining == 0) {
                StartDmc();
            }
            mDmcIrq = false;
            break;
        case 0x4017:
            mFiveStep = v & 0x80;
            mIrqInhibit = v & 0x40;
            if (mIrqInhibit) {
                mFrameIrq = false;
            }
            mFrameCycle = 0;
            if (mFiveStep) {
                QuarterFrame();
                HalfFrame();
            }
            break;
        default:
            break;
    }
}

uint8_t Apu::ReadStatus() {
    uint8_t v = (mPulse[0].length > 0 ? 0x01 : 0) | (mPulse[1].length > 0 ? 0x02 : 0) |
                (mTri.length > 0 ? 0x04 : 0) | (mNoise.length > 0 ? 0x08 : 0) | (mDmc.remaining > 0 ? 0x10 : 0) |
                (mFrameIrq ? 0x40 : 0) | (mDmcIrq ? 0x80 : 0);
    mFrameIrq = false;
    return v;
}

void Apu::QuarterFrame() {
    mPulse[0].env.Clock();
    mPulse[1].env.Clock();
    mNoise.env.Clock();
    if (mTri.reload) {
        mTri.linear = mTri.linearReload;
    } else if (mTri.linear > 0) {
        mTri.linear--;
    }
    if (!mTri.control) {
        mTri.reload = false;
    }
}

void Apu::HalfFrame() {
    for (Pulse& p : mPulse) {
        if (!p.env.loop && p.length > 0) {
            p.length--;
        }
        p.ClockSweep();
    }
    if (!mTri.control && mTri.length > 0) {
        mTri.length--;
    }
    if (!mNoise.env.loop && mNoise.length > 0) {
        mNoise.length--;
    }
}

void Apu::StepFrameCounter() {
    mFrameCycle++;
    switch (mFrameCycle) {
        case kStep1:
        case kStep3:
            QuarterFrame();
            break;
        case kStep2:
            QuarterFrame();
            HalfFrame();
            break;
        case kStep4:
            if (!mFiveStep) {
                QuarterFrame();
                HalfFrame();
                if (!mIrqInhibit) {
                    mFrameIrq = true;
                }
                mFrameCycle = 0;
            }
            break;
        case kStep5:
            QuarterFrame();
            HalfFrame();
            mFrameCycle = 0;
            break;
        default:
            break;
    }
}

void Apu::StartDmc() {
    mDmc.addr = mDmc.sampleAddr;
    mDmc.remaining = mDmc.sampleLength;
}

void Apu::FetchDmc() {
    mDmc.buffer = mRead(mDmc.addr);
    mDmc.bufferFull = true;
    mDmc.addr = mDmc.addr == 0xFFFF ? 0x8000 : mDmc.addr + 1;
    if (--mDmc.remaining == 0) {
        if (mDmc.loop) {
            StartDmc();
        } else if (mDmc.irqEnabled) {
            mDmcIrq = true;
        }
    }
}

float Apu::Mix() const {
    uint8_t p = mPulse[0].Output() + mPulse[1].Output();
    uint8_t t = mTri.step < 16 ? 15 - mTri.step : mTri.step - 16;
    uint8_t n = (mNoise.length > 0 && !(mNoise.shift & 1)) ? mNoise.env.Volume() : 0;
    return kMix.pulse[p] + kMix.tnd[3 * t + 2 * n + mDmc.output];
}

void Apu::Clock(int cycles) {
    static const float kHp90 = OnePoleAlpha(90.0, true);
    static const float kHp440 = OnePoleAlpha(440.0, true);
    static const float kLp14k = OnePoleAlpha(14000.0, false);

    for (int c = 0; c < cycles; c++) {
        // Triangle: clocked every CPU cycle. Very high (inaudible) periods are skipped, as most emulators
        // do, to avoid aliasing.
        if (mTri.timer == 0) {
            mTri.timer = mTri.period;
            if (mTri.length > 0 && mTri.linear > 0 && mTri.period >= 2) {
                mTri.step = (mTri.step + 1) & 31;
            }
        } else {
            mTri.timer--;
        }

        // Pulses: clocked every other CPU cycle.
        mOddCycle = !mOddCycle;
        if (mOddCycle) {
            for (Pulse& p : mPulse) {
                if (p.timer == 0) {
                    p.timer = p.period;
                    p.step = (p.step + 1) & 7;
                } else {
                    p.timer--;
                }
            }
        }

        if (mNoise.timer == 0) {
            mNoise.timer = mNoise.period - 1;
            uint16_t fb = (mNoise.shift ^ (mNoise.shift >> (mNoise.mode ? 6 : 1))) & 1;
            mNoise.shift = (mNoise.shift >> 1) | (fb << 14);
        } else {
            mNoise.timer--;
        }

        if (mDmc.timer == 0) {
            mDmc.timer = mDmc.period - 1;
            if (!mDmc.silent) {
                if (mDmc.shift & 1) {
                    if (mDmc.output <= 125) {
                        mDmc.output += 2;
                    }
                } else if (mDmc.output >= 2) {
                    mDmc.output -= 2;
                }
            }
            mDmc.shift >>= 1;
            if (--mDmc.bits == 0) {
                mDmc.bits = 8;
                if (mDmc.bufferFull) {
                    mDmc.silent = false;
                    mDmc.shift = mDmc.buffer;
                    mDmc.bufferFull = false;
                } else {
                    mDmc.silent = true;
                }
            }
        } else {
            mDmc.timer--;
        }
        if (!mDmc.bufferFull && mDmc.remaining > 0) {
            FetchDmc();
        }

        StepFrameCounter();

        mAccum += Mix();
        mAccumCount++;
        mSampleClock += 1.0;
        if (mSampleClock >= kCyclesPerSample) {
            mSampleClock -= kCyclesPerSample;
            float x = (float)(mAccum / mAccumCount);
            mAccum = 0;
            mAccumCount = 0;
            mHp90Out = kHp90 * (mHp90Out + x - mHp90Prev);
            mHp90Prev = x;
            mHp440Out = kHp440 * (mHp440Out + mHp90Out - mHp440Prev);
            mHp440Prev = mHp90Out;
            mLpOut += kLp14k * (mHp440Out - mLpOut);
            float s = std::clamp(mLpOut * 3.0f, -1.0f, 1.0f);
            mSamples.push_back((int16_t)(s * 32000.0f));
        }
    }
}

} // namespace Nes
