#include "Cpu6502.h"

namespace Nes {

void Cpu6502::Reset() {
    mA = mX = mY = 0;
    mS = 0xFD;
    mP = I | U;
    mPC = Rd16(0xFFFC);
    mCycles += 7;
}

void Cpu6502::Interrupt(uint16_t vector, bool brk) {
    Push(mPC >> 8);
    Push(mPC & 0xFF);
    Push((mP | U | (brk ? B : 0)) & (brk ? 0xFF : ~B));
    mP |= I;
    mPC = Rd16(vector);
    mCycles += 7;
}

void Cpu6502::Nmi() {
    Interrupt(0xFFFA, false);
}

void Cpu6502::Irq() {
    if (!(mP & I)) {
        Interrupt(0xFFFE, false);
    }
}

uint16_t Cpu6502::Zp() {
    return Rd(mPC++);
}
uint16_t Cpu6502::ZpX() {
    return (Rd(mPC++) + mX) & 0xFF;
}
uint16_t Cpu6502::ZpY() {
    return (Rd(mPC++) + mY) & 0xFF;
}
uint16_t Cpu6502::Abs() {
    uint16_t a = Rd16(mPC);
    mPC += 2;
    return a;
}
uint16_t Cpu6502::AbsX(bool penalty) {
    uint16_t base = Abs();
    uint16_t a = base + mX;
    if (penalty && ((base ^ a) & 0xFF00)) {
        mExtra++;
    }
    return a;
}
uint16_t Cpu6502::AbsY(bool penalty) {
    uint16_t base = Abs();
    uint16_t a = base + mY;
    if (penalty && ((base ^ a) & 0xFF00)) {
        mExtra++;
    }
    return a;
}
uint16_t Cpu6502::IndX() {
    uint8_t zp = (Rd(mPC++) + mX) & 0xFF;
    return Rd(zp) | (Rd((zp + 1) & 0xFF) << 8);
}
uint16_t Cpu6502::IndY(bool penalty) {
    uint8_t zp = Rd(mPC++);
    uint16_t base = Rd(zp) | (Rd((zp + 1) & 0xFF) << 8);
    uint16_t a = base + mY;
    if (penalty && ((base ^ a) & 0xFF00)) {
        mExtra++;
    }
    return a;
}

void Cpu6502::Adc(uint8_t v) {
    uint16_t sum = mA + v + (mP & C);
    SetFlag(C, sum > 0xFF);
    SetFlag(V, (~(mA ^ v) & (mA ^ sum) & 0x80) != 0);
    mA = (uint8_t)sum;
    SetZN(mA);
}

void Cpu6502::Cmp(uint8_t reg, uint8_t v) {
    uint8_t r = reg - v;
    SetFlag(C, reg >= v);
    SetZN(r);
}

uint8_t Cpu6502::Asl(uint8_t v) {
    SetFlag(C, v & 0x80);
    v <<= 1;
    SetZN(v);
    return v;
}
uint8_t Cpu6502::Lsr(uint8_t v) {
    SetFlag(C, v & 0x01);
    v >>= 1;
    SetZN(v);
    return v;
}
uint8_t Cpu6502::Rol(uint8_t v) {
    uint8_t c = mP & C;
    SetFlag(C, v & 0x80);
    v = (v << 1) | c;
    SetZN(v);
    return v;
}
uint8_t Cpu6502::Ror(uint8_t v) {
    uint8_t c = (mP & C) << 7;
    SetFlag(C, v & 0x01);
    v = (v >> 1) | c;
    SetZN(v);
    return v;
}

void Cpu6502::Branch(bool cond) {
    int8_t off = (int8_t)Rd(mPC++);
    if (cond) {
        uint16_t target = mPC + off;
        mExtra += ((target ^ mPC) & 0xFF00) ? 2 : 1;
        mPC = target;
    }
}

int Cpu6502::Step() {
    mExtra = 0;
    uint8_t op = Rd(mPC++);
    int cycles = 2;
    uint16_t a;
    uint8_t v;

// Read-modify-write helper: performs the dummy write the 6502 does (some hardware relies on it).
#define RMW(addrExpr, fn, cyc)  \
    a = addrExpr;               \
    v = Rd(a);                  \
    Wr(a, v);                   \
    Wr(a, fn(v));               \
    cycles = cyc;               \
    break

    switch (op) {
        // --- Loads ---
        case 0xA9: mA = Rd(mPC++); SetZN(mA); cycles = 2; break;
        case 0xA5: mA = Rd(Zp()); SetZN(mA); cycles = 3; break;
        case 0xB5: mA = Rd(ZpX()); SetZN(mA); cycles = 4; break;
        case 0xAD: mA = Rd(Abs()); SetZN(mA); cycles = 4; break;
        case 0xBD: mA = Rd(AbsX(true)); SetZN(mA); cycles = 4; break;
        case 0xB9: mA = Rd(AbsY(true)); SetZN(mA); cycles = 4; break;
        case 0xA1: mA = Rd(IndX()); SetZN(mA); cycles = 6; break;
        case 0xB1: mA = Rd(IndY(true)); SetZN(mA); cycles = 5; break;
        case 0xA2: mX = Rd(mPC++); SetZN(mX); cycles = 2; break;
        case 0xA6: mX = Rd(Zp()); SetZN(mX); cycles = 3; break;
        case 0xB6: mX = Rd(ZpY()); SetZN(mX); cycles = 4; break;
        case 0xAE: mX = Rd(Abs()); SetZN(mX); cycles = 4; break;
        case 0xBE: mX = Rd(AbsY(true)); SetZN(mX); cycles = 4; break;
        case 0xA0: mY = Rd(mPC++); SetZN(mY); cycles = 2; break;
        case 0xA4: mY = Rd(Zp()); SetZN(mY); cycles = 3; break;
        case 0xB4: mY = Rd(ZpX()); SetZN(mY); cycles = 4; break;
        case 0xAC: mY = Rd(Abs()); SetZN(mY); cycles = 4; break;
        case 0xBC: mY = Rd(AbsX(true)); SetZN(mY); cycles = 4; break;

        // --- Stores ---
        case 0x85: Wr(Zp(), mA); cycles = 3; break;
        case 0x95: Wr(ZpX(), mA); cycles = 4; break;
        case 0x8D: Wr(Abs(), mA); cycles = 4; break;
        case 0x9D: Wr(AbsX(false), mA); cycles = 5; break;
        case 0x99: Wr(AbsY(false), mA); cycles = 5; break;
        case 0x81: Wr(IndX(), mA); cycles = 6; break;
        case 0x91: Wr(IndY(false), mA); cycles = 6; break;
        case 0x86: Wr(Zp(), mX); cycles = 3; break;
        case 0x96: Wr(ZpY(), mX); cycles = 4; break;
        case 0x8E: Wr(Abs(), mX); cycles = 4; break;
        case 0x84: Wr(Zp(), mY); cycles = 3; break;
        case 0x94: Wr(ZpX(), mY); cycles = 4; break;
        case 0x8C: Wr(Abs(), mY); cycles = 4; break;

        // --- Transfers ---
        case 0xAA: mX = mA; SetZN(mX); break;
        case 0xA8: mY = mA; SetZN(mY); break;
        case 0x8A: mA = mX; SetZN(mA); break;
        case 0x98: mA = mY; SetZN(mA); break;
        case 0xBA: mX = mS; SetZN(mX); break;
        case 0x9A: mS = mX; break;

        // --- Stack ---
        case 0x48: Push(mA); cycles = 3; break;
        case 0x08: Push(mP | B | U); cycles = 3; break;
        case 0x68: mA = Pull(); SetZN(mA); cycles = 4; break;
        case 0x28: mP = (Pull() & ~B) | U; cycles = 4; break;

        // --- Logic ---
        case 0x29: mA &= Rd(mPC++); SetZN(mA); cycles = 2; break;
        case 0x25: mA &= Rd(Zp()); SetZN(mA); cycles = 3; break;
        case 0x35: mA &= Rd(ZpX()); SetZN(mA); cycles = 4; break;
        case 0x2D: mA &= Rd(Abs()); SetZN(mA); cycles = 4; break;
        case 0x3D: mA &= Rd(AbsX(true)); SetZN(mA); cycles = 4; break;
        case 0x39: mA &= Rd(AbsY(true)); SetZN(mA); cycles = 4; break;
        case 0x21: mA &= Rd(IndX()); SetZN(mA); cycles = 6; break;
        case 0x31: mA &= Rd(IndY(true)); SetZN(mA); cycles = 5; break;
        case 0x09: mA |= Rd(mPC++); SetZN(mA); cycles = 2; break;
        case 0x05: mA |= Rd(Zp()); SetZN(mA); cycles = 3; break;
        case 0x15: mA |= Rd(ZpX()); SetZN(mA); cycles = 4; break;
        case 0x0D: mA |= Rd(Abs()); SetZN(mA); cycles = 4; break;
        case 0x1D: mA |= Rd(AbsX(true)); SetZN(mA); cycles = 4; break;
        case 0x19: mA |= Rd(AbsY(true)); SetZN(mA); cycles = 4; break;
        case 0x01: mA |= Rd(IndX()); SetZN(mA); cycles = 6; break;
        case 0x11: mA |= Rd(IndY(true)); SetZN(mA); cycles = 5; break;
        case 0x49: mA ^= Rd(mPC++); SetZN(mA); cycles = 2; break;
        case 0x45: mA ^= Rd(Zp()); SetZN(mA); cycles = 3; break;
        case 0x55: mA ^= Rd(ZpX()); SetZN(mA); cycles = 4; break;
        case 0x4D: mA ^= Rd(Abs()); SetZN(mA); cycles = 4; break;
        case 0x5D: mA ^= Rd(AbsX(true)); SetZN(mA); cycles = 4; break;
        case 0x59: mA ^= Rd(AbsY(true)); SetZN(mA); cycles = 4; break;
        case 0x41: mA ^= Rd(IndX()); SetZN(mA); cycles = 6; break;
        case 0x51: mA ^= Rd(IndY(true)); SetZN(mA); cycles = 5; break;
        case 0x24: v = Rd(Zp()); SetFlag(Z, (mA & v) == 0); mP = (mP & 0x3F) | (v & 0xC0); cycles = 3; break;
        case 0x2C: v = Rd(Abs()); SetFlag(Z, (mA & v) == 0); mP = (mP & 0x3F) | (v & 0xC0); cycles = 4; break;

        // --- Arithmetic ---
        case 0x69: Adc(Rd(mPC++)); cycles = 2; break;
        case 0x65: Adc(Rd(Zp())); cycles = 3; break;
        case 0x75: Adc(Rd(ZpX())); cycles = 4; break;
        case 0x6D: Adc(Rd(Abs())); cycles = 4; break;
        case 0x7D: Adc(Rd(AbsX(true))); cycles = 4; break;
        case 0x79: Adc(Rd(AbsY(true))); cycles = 4; break;
        case 0x61: Adc(Rd(IndX())); cycles = 6; break;
        case 0x71: Adc(Rd(IndY(true))); cycles = 5; break;
        case 0xE9: Adc(~Rd(mPC++)); cycles = 2; break;
        case 0xE5: Adc(~Rd(Zp())); cycles = 3; break;
        case 0xF5: Adc(~Rd(ZpX())); cycles = 4; break;
        case 0xED: Adc(~Rd(Abs())); cycles = 4; break;
        case 0xFD: Adc(~Rd(AbsX(true))); cycles = 4; break;
        case 0xF9: Adc(~Rd(AbsY(true))); cycles = 4; break;
        case 0xE1: Adc(~Rd(IndX())); cycles = 6; break;
        case 0xF1: Adc(~Rd(IndY(true))); cycles = 5; break;

        // --- Compares ---
        case 0xC9: Cmp(mA, Rd(mPC++)); cycles = 2; break;
        case 0xC5: Cmp(mA, Rd(Zp())); cycles = 3; break;
        case 0xD5: Cmp(mA, Rd(ZpX())); cycles = 4; break;
        case 0xCD: Cmp(mA, Rd(Abs())); cycles = 4; break;
        case 0xDD: Cmp(mA, Rd(AbsX(true))); cycles = 4; break;
        case 0xD9: Cmp(mA, Rd(AbsY(true))); cycles = 4; break;
        case 0xC1: Cmp(mA, Rd(IndX())); cycles = 6; break;
        case 0xD1: Cmp(mA, Rd(IndY(true))); cycles = 5; break;
        case 0xE0: Cmp(mX, Rd(mPC++)); cycles = 2; break;
        case 0xE4: Cmp(mX, Rd(Zp())); cycles = 3; break;
        case 0xEC: Cmp(mX, Rd(Abs())); cycles = 4; break;
        case 0xC0: Cmp(mY, Rd(mPC++)); cycles = 2; break;
        case 0xC4: Cmp(mY, Rd(Zp())); cycles = 3; break;
        case 0xCC: Cmp(mY, Rd(Abs())); cycles = 4; break;

        // --- Increments / decrements ---
        case 0xE6: { auto inc = [this](uint8_t x) { x++; SetZN(x); return x; }; RMW(Zp(), inc, 5); }
        case 0xF6: { auto inc = [this](uint8_t x) { x++; SetZN(x); return x; }; RMW(ZpX(), inc, 6); }
        case 0xEE: { auto inc = [this](uint8_t x) { x++; SetZN(x); return x; }; RMW(Abs(), inc, 6); }
        case 0xFE: { auto inc = [this](uint8_t x) { x++; SetZN(x); return x; }; RMW(AbsX(false), inc, 7); }
        case 0xC6: { auto dec = [this](uint8_t x) { x--; SetZN(x); return x; }; RMW(Zp(), dec, 5); }
        case 0xD6: { auto dec = [this](uint8_t x) { x--; SetZN(x); return x; }; RMW(ZpX(), dec, 6); }
        case 0xCE: { auto dec = [this](uint8_t x) { x--; SetZN(x); return x; }; RMW(Abs(), dec, 6); }
        case 0xDE: { auto dec = [this](uint8_t x) { x--; SetZN(x); return x; }; RMW(AbsX(false), dec, 7); }
        case 0xE8: mX++; SetZN(mX); break;
        case 0xC8: mY++; SetZN(mY); break;
        case 0xCA: mX--; SetZN(mX); break;
        case 0x88: mY--; SetZN(mY); break;

        // --- Shifts ---
        case 0x0A: mA = Asl(mA); break;
        case 0x4A: mA = Lsr(mA); break;
        case 0x2A: mA = Rol(mA); break;
        case 0x6A: mA = Ror(mA); break;
        case 0x06: { auto f = [this](uint8_t x) { return Asl(x); }; RMW(Zp(), f, 5); }
        case 0x16: { auto f = [this](uint8_t x) { return Asl(x); }; RMW(ZpX(), f, 6); }
        case 0x0E: { auto f = [this](uint8_t x) { return Asl(x); }; RMW(Abs(), f, 6); }
        case 0x1E: { auto f = [this](uint8_t x) { return Asl(x); }; RMW(AbsX(false), f, 7); }
        case 0x46: { auto f = [this](uint8_t x) { return Lsr(x); }; RMW(Zp(), f, 5); }
        case 0x56: { auto f = [this](uint8_t x) { return Lsr(x); }; RMW(ZpX(), f, 6); }
        case 0x4E: { auto f = [this](uint8_t x) { return Lsr(x); }; RMW(Abs(), f, 6); }
        case 0x5E: { auto f = [this](uint8_t x) { return Lsr(x); }; RMW(AbsX(false), f, 7); }
        case 0x26: { auto f = [this](uint8_t x) { return Rol(x); }; RMW(Zp(), f, 5); }
        case 0x36: { auto f = [this](uint8_t x) { return Rol(x); }; RMW(ZpX(), f, 6); }
        case 0x2E: { auto f = [this](uint8_t x) { return Rol(x); }; RMW(Abs(), f, 6); }
        case 0x3E: { auto f = [this](uint8_t x) { return Rol(x); }; RMW(AbsX(false), f, 7); }
        case 0x66: { auto f = [this](uint8_t x) { return Ror(x); }; RMW(Zp(), f, 5); }
        case 0x76: { auto f = [this](uint8_t x) { return Ror(x); }; RMW(ZpX(), f, 6); }
        case 0x6E: { auto f = [this](uint8_t x) { return Ror(x); }; RMW(Abs(), f, 6); }
        case 0x7E: { auto f = [this](uint8_t x) { return Ror(x); }; RMW(AbsX(false), f, 7); }

        // --- Jumps / calls ---
        case 0x4C: mPC = Rd16(mPC); cycles = 3; break;
        case 0x6C: mPC = Rd16Bug(Rd16(mPC)); cycles = 5; break;
        case 0x20: {
            uint16_t target = Rd16(mPC);
            uint16_t ret = mPC + 1;
            Push(ret >> 8);
            Push(ret & 0xFF);
            mPC = target;
            cycles = 6;
            break;
        }
        case 0x60: {
            uint16_t lo = Pull();
            uint16_t hi = Pull();
            mPC = ((hi << 8) | lo) + 1;
            cycles = 6;
            break;
        }
        case 0x40: {
            mP = (Pull() & ~B) | U;
            uint16_t lo = Pull();
            uint16_t hi = Pull();
            mPC = (hi << 8) | lo;
            cycles = 6;
            break;
        }
        case 0x00:
            mPC++;
            Interrupt(0xFFFE, true);
            cycles = 0; // Interrupt() already counted 7
            break;

        // --- Branches ---
        case 0x10: Branch(!(mP & N)); break;
        case 0x30: Branch(mP & N); break;
        case 0x50: Branch(!(mP & V)); break;
        case 0x70: Branch(mP & V); break;
        case 0x90: Branch(!(mP & C)); break;
        case 0xB0: Branch(mP & C); break;
        case 0xD0: Branch(!(mP & Z)); break;
        case 0xF0: Branch(mP & Z); break;

        // --- Flags ---
        case 0x18: mP &= ~C; break;
        case 0x38: mP |= C; break;
        case 0x58: mP &= ~I; break;
        case 0x78: mP |= I; break;
        case 0xB8: mP &= ~V; break;
        case 0xD8: mP &= ~D; break;
        case 0xF8: mP |= D; break;

        case 0xEA: break; // NOP

        default:
            mJammed = true;
            break;
    }
#undef RMW
    cycles += mExtra;
    mCycles += cycles;
    return cycles;
}

} // namespace Nes
