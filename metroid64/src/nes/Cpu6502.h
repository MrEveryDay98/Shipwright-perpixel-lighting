#pragma once

#include <cstdint>

namespace Nes {

// Memory interface the CPU runs against.
class Bus {
  public:
    virtual ~Bus() = default;
    virtual uint8_t Read(uint16_t addr) = 0;
    virtual void Write(uint16_t addr, uint8_t value) = 0;
};

// MOS 6502 (NES 2A03 variant: no decimal mode). Official opcodes only, which is all Metroid uses.
class Cpu6502 {
  public:
    explicit Cpu6502(Bus& bus) : mBus(bus) {
    }

    void Reset();
    void Nmi();
    void Irq();
    // Executes one instruction and returns the cycles it took.
    int Step();

    uint16_t PC() const {
        return mPC;
    }
    uint64_t Cycles() const {
        return mCycles;
    }
    void AddCycles(int n) {
        mCycles += n;
    }
    // Set when an unknown opcode was hit (the CPU then treats it as a 2-cycle NOP).
    bool Jammed() const {
        return mJammed;
    }
    uint8_t A() const {
        return mA;
    }
    uint8_t S() const {
        return mS;
    }

  private:
    enum Flag : uint8_t { C = 0x01, Z = 0x02, I = 0x04, D = 0x08, B = 0x10, U = 0x20, V = 0x40, N = 0x80 };

    uint8_t Rd(uint16_t a) {
        return mBus.Read(a);
    }
    void Wr(uint16_t a, uint8_t v) {
        mBus.Write(a, v);
    }
    uint16_t Rd16(uint16_t a) {
        return Rd(a) | (Rd(a + 1) << 8);
    }
    // 6502 JMP ($xxFF) bug: the high byte wraps within the page.
    uint16_t Rd16Bug(uint16_t a) {
        return Rd(a) | (Rd((a & 0xFF00) | ((a + 1) & 0x00FF)) << 8);
    }
    void Push(uint8_t v) {
        Wr(0x0100 | mS--, v);
    }
    uint8_t Pull() {
        return Rd(0x0100 | ++mS);
    }
    void SetZN(uint8_t v) {
        mP = (mP & ~(Z | N)) | (v == 0 ? Z : 0) | (v & N);
    }
    void SetFlag(uint8_t f, bool on) {
        mP = on ? (mP | f) : (mP & ~f);
    }
    void Interrupt(uint16_t vector, bool brk);

    // Addressing modes: return the effective address; page-cross adds a cycle where it applies.
    uint16_t AbsX(bool penalty);
    uint16_t AbsY(bool penalty);
    uint16_t IndY(bool penalty);
    uint16_t IndX();
    uint16_t Zp();
    uint16_t ZpX();
    uint16_t ZpY();
    uint16_t Abs();

    void Adc(uint8_t v);
    void Cmp(uint8_t reg, uint8_t v);
    uint8_t Asl(uint8_t v);
    uint8_t Lsr(uint8_t v);
    uint8_t Rol(uint8_t v);
    uint8_t Ror(uint8_t v);
    void Branch(bool cond);

    Bus& mBus;
    uint16_t mPC = 0;
    uint8_t mA = 0, mX = 0, mY = 0, mS = 0xFD, mP = I | U;
    uint64_t mCycles = 0;
    int mExtra = 0;
    bool mJammed = false;
};

} // namespace Nes
