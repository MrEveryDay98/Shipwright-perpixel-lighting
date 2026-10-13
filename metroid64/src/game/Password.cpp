// Bank 0: passwords. The game state is packed into 18 password bytes ($6988-$6999): 59 bits for the
// unique items (power-ups, red doors, the Zebetites and Mother Brain), the start area, the suit, the
// gear, missiles, Samus's age and the statues, a random shift count and a checksum. The bytes are
// rotated by the shift count and spread over 24 six-bit characters ($699A-$69B1).
// Routine names and addresses follow the community disassembly (Bank00.asm, $8B79-$9028).

#include "game/Title.h"

#include "game/Engine.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

namespace {
constexpr uint16_t kNARPASSWORDTbl = 0x8E07; // "NARPASSWORD00000"
constexpr uint16_t kPswrdBitmaskTbl = 0x9021;
constexpr uint16_t kItemData = 0x9029; // the 59 unique items, two bytes each
} // namespace

// --- $8B79 Unique items <-> password bits -----------------------------------------------------------

// $8B79: sets the password bit of every unique item in the history.
void ProcessUniqueItems() {
    GenByte03 = NumUniqueItems;
    GenByte04 = 0;
    do {
        uint8_t y = GenByte04;
        y++;
        GenWord00LB = UnqItmHist[y - 1];
        y++;
        GenWord00UB = UnqItmHist[y - 1];
        GenByte04 = y;
        UniqueItemSearch();
    } while (GenByte04 < GenByte03);
}

// $8B9C: looks the item in $00/$01 up in ItemData. The original searches 60 entries; the 60th is
// whatever follows the table.
void UniqueItemSearch() {
    for (uint8_t x = 0; x < 0x3C; x++) {
        uint8_t y = (uint8_t)(x << 1);
        if (Read((uint16_t)(kItemData + y)) == GenWord00LB && Read((uint16_t)(kItemData + y + 1)) == GenWord00UB) {
            UniqueItemFound(x);
            return;
        }
    }
}

// $8BB5
void UniqueItemFound(uint8_t x) {
    Mem(0x05) = x >> 3;
    Mem(0x02) = (uint8_t)(Mem(0x05) << 3);
    Mem(0x06) = (uint8_t)(x - Mem(0x02));
    PasswordByte00[Mem(0x05)] |= Read((uint16_t)(kPswrdBitmaskTbl + Mem(0x06)));
}

// $8BD4: rebuilds the unique item history from the password bits.
void LoadUniqueItems() {
    NumUniqueItems = 0;
    Mem(0x05) = 0; // password byte
    Mem(0x06) = 0; // bit in it
    Mem(0x07) = 0x3B;
    Mem(0x08) = PasswordByte00[Mem(0x05)];
    Mem(0x09) = 0; // items checked
    // (The original has code here for starting mid-byte, which $06 = 0 skips.)
    for (;;) {
        // PrcsNewItemByte. The bit rotated into $08 is the carry left by the comparisons below
        // (clear), or the caller's carry on the first pass; it never reaches a tested bit.
        bool c = false;
        Mem(0x08) = Ror(Mem(0x08), c);
        if (c) {
            SamusHasItem();
        }
        if (Mem(0x06) < 0x07) {
            Mem(0x06)++;
            Mem(0x09)++;
            if (Mem(0x09) >= Mem(0x07)) {
                return;
            }
            continue;
        }
        Mem(0x06) = 0;
        Mem(0x05)++;
        Mem(0x09)++;
        if (Mem(0x09) >= Mem(0x07)) {
            return;
        }
        // ProcessNextItem
        Mem(0x08) = PasswordByte00[Mem(0x05)];
    }
}

// $8C39: adds item ($05 * 8 + $06) to the history.
void SamusHasItem() {
    uint8_t y = (uint8_t)(((uint8_t)(Mem(0x05) << 3) + Mem(0x06)) << 1);
    Mem(0x01) = Read((uint16_t)(kItemData + y + 1));
    GenByte00 = Read((uint16_t)(kItemData + y));
    y = NumUniqueItems;
    UnqItmHist[y] = GenByte00;
    y++;
    UnqItmHist[y] = Mem(0x01);
    y++;
    NumUniqueItems = y;
}

// --- $8C5E Checking and making passwords -------------------------------------------------------------

void CheckPassword() {
    bool c = ConsolidatePassword();
    if (!ValidatePassword(c)) {
        InitializeGame();
        return;
    }
    MultiSFXFlag |= 0x01; // incorrect password sound
    Timer3 = 0x0C;        // "ERROR TRY AGAIN" for 120 frames
    TitleRoutine = 0x18;  // EnterPassword
}

// $8C7A
void CalculatePassword() {
    for (int y = 0x0F; y >= 0; y--) {
        PasswordByte00[y] = 0;
        PasswordChar00[y] = 0;
    }
    ProcessUniqueItems();
    if (PasswordByte07 & 0x04) {
        // Mother Brain was defeated: the password brings her, the Zebetites and Tourian's doors back.
        PasswordByte07 = 0;
        PasswordByte06 &= 0x03;
    }
    uint8_t a = InArea & 0x3F;
    if (JustInBailey != 0) {
        a |= 0x80;
    }
    PasswordByte08 = a;
    PasswordByte09 = SamusGear;
    PasswordByte0A = MissileCount;
    GenByte00 = 0;
    if (KraidStatueStat & 0x80) {
        GenByte00 |= 0x80; // Kraid's statue up
    }
    if (KraidStatueStat & 0x01) {
        GenByte00 |= 0x40; // Kraid defeated
    }
    if (RidlyStatueStat & 0x80) {
        GenByte00 |= 0x20;
    }
    if (RidlyStatueStat & 0x02) {
        GenByte00 |= 0x10;
    }
    PasswordByte0F = GenByte00;
    for (int y = 3; y >= 0; y--) {
        PasswordByte0B[y] = SamusAgeLo[y];
    }
    do {
        RandomNumbers();
    } while ((RandomNumber1 & 0x0F) == 0);
    PasswordByte10 = RandomNumber1 & 0x0F;
    PswrdChksmNScrmbl();
    LoadPasswordChar();
}

// $8D12
void LoadPasswordData() {
    if (NARPASSWORD != 0) {
        return;
    }
    LoadUniqueItems();
    LdTanksAndMissiles();
    JustInBailey = (PasswordByte08 & 0x80) ? 1 : 0;
    InArea = PasswordByte08 & 0x3F;
    for (int y = 3; y >= 0; y--) {
        SamusAgeLo[y] = PasswordByte0B[y];
    }
}

// $8D3D
void LdTanksAndMissiles() {
    SamusGear = PasswordByte09;
    MissileCount = PasswordByte0A;
    GenByte00 = 0;
    Mem(0x02) = 0;
    if (PasswordByte0F & 0x80) {
        GenByte00 |= 0x80;
    }
    if (PasswordByte0F & 0x40) {
        GenByte00 |= 0x01;
    }
    KraidStatueStat = GenByte00;
    if (PasswordByte0F & 0x20) {
        Mem(0x02) |= 0x80;
    }
    if (PasswordByte0F & 0x10) {
        Mem(0x02) |= 0x02;
    }
    RidlyStatueStat = Mem(0x02);

    // Count energy tanks ($00) and missile packs ($02) in the history.
    GenByte00 = 0;
    Mem(0x02) = 0;
    for (uint8_t y = 0; y < 0x84; y += 2) {
        uint8_t a = UnqItmHist[y + 1] & 0xFC;
        if (a == 0x20) {
            GenByte00++;
        } else if (a == 0x24) {
            Mem(0x02)++;
        }
    }
    uint8_t a = GenByte00;
    bool c = Cmp(a, 0x06);
    if (c) {
        a = 0x06;
    }
    TankCount = a;

    // Five missiles per pack, 75 more for each boss. The carry of the tank comparison above is added
    // in when there is no pack.
    a = 0;
    uint8_t y = Mem(0x02);
    if (y != 0) {
        c = false;
        do {
            a = Adc(a, 0x05, c);
        } while (--y != 0);
    }
    if (KraidStatueStat != 0) {
        a = Adc(a, 0x4B, c);
        if (c) {
            MaxMissiles = 0xFF;
            return;
        }
    }
    if (RidlyStatueStat != 0) {
        a = Adc(a, 0x4B, c);
        if (c) {
            a = 0xFF;
        }
    }
    MaxMissiles = a;
}

// $8DDE: returns the carry, set when the password is wrong. NARPASSWORD ("NARPASSWORD00000", the
// last 8 characters don't matter) turns on invincible Samus; once it is on, passwords are checked as
// usual.
bool ValidatePassword(bool carry) {
    if (NARPASSWORD == 0) {
        bool narpassword = true;
        for (int y = 0x0F; y >= 0; y--) {
            uint8_t ch = PasswordChar00[y];
            uint8_t want = Read((uint16_t)(kNARPASSWORDTbl + y));
            if (ch != want) {
                carry = Cmp(ch, want);
                narpassword = false;
                break;
            }
        }
        if (narpassword) {
            NARPASSWORD = 1;
            return false;
        }
    }
    UnscramblePassword(carry);
    bool c = false;
    uint8_t sum = PasswordChecksum(c);
    return sum != PasswordByte11;
}

// $8E17
void PswrdChksmNScrmbl() {
    bool c = false;
    PasswordByte11 = PasswordChecksum(c);
    PasswordScramble(c);
}

// $8E21: sum of $6988-$6998; the carry of the last addition is left for PasswordScramble.
uint8_t PasswordChecksum(bool& carry) {
    uint8_t a = 0;
    for (int y = 0x10; y >= 0; y--) {
        carry = false;
        a = Adc(a, PasswordByte00[y], carry);
    }
    return a;
}

// $8E2D: rotates the 16 bytes $6988-$6997 right, as one 128-bit number, PasswordByte10 times.
void PasswordScramble(bool carry) {
    Mem(0x02) = PasswordByte10;
    do {
        GenByte00 = PasswordByte00;
        for (int x = 0; x < 0x10; x++) {
            PasswordByte00[x] = Ror(PasswordByte00[x], carry);
        }
        GenByte00 = Ror(GenByte00, carry);
        PasswordByte00 = GenByte00;
    } while (--Mem(0x02) != 0);
}

// $8E4E: the reverse, rotating left.
void UnscramblePassword(bool carry) {
    Mem(0x02) = PasswordByte10;
    do {
        GenByte00 = PasswordByte0F;
        for (int x = 0x0F; x >= 0; x--) {
            PasswordByte00[x] = Rol(PasswordByte00[x], carry);
        }
        GenByte00 = Rol(GenByte00, carry);
        PasswordByte0F = GenByte00;
    } while (--Mem(0x02) != 0);
}

// --- $8E6C Bytes <-> characters ----------------------------------------------------------------------

// $8F2D
static uint8_t SixUpperBits(uint8_t y) {
    return PasswordByte00[y] >> 2;
}

// $8F33
static uint8_t TwoLoAndFourUp(uint8_t y) {
    GenByte00 = (uint8_t)((PasswordByte00[y] & 0x03) << 4);
    return (PasswordByte01[y] >> 4) | GenByte00;
}

// $8F46
static uint8_t FourLoAndTwoUp(uint8_t y) {
    GenByte00 = (uint8_t)((PasswordByte00[y] & 0x0F) << 2);
    return ((PasswordByte01[y] >> 6) & 0x03) | GenByte00;
}

// $8F5A
static uint8_t SixLowerBits(uint8_t y) {
    return PasswordByte00[y] & 0x3F;
}

// $8E6C: every three password bytes make four characters.
void LoadPasswordChar() {
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t b = (uint8_t)(i * 3);
        uint8_t c = (uint8_t)(i * 4);
        PasswordChar00[c] = SixUpperBits(b);
        PasswordChar00[c + 1] = TwoLoAndFourUp(b);
        PasswordChar00[c + 2] = FourLoAndTwoUp(b + 1);
        PasswordChar00[c + 3] = SixLowerBits(b + 2);
    }
}

// $8FF1
static uint8_t SixLowerAndTwoUpper(uint8_t y) {
    GenByte00 = (uint8_t)(PasswordChar00[y] << 2);
    return (PasswordChar01[y] >> 4) | GenByte00;
}

// $9001
static uint8_t FourLoNFiveThruTwo(uint8_t y) {
    GenByte00 = (uint8_t)(PasswordChar00[y] << 4);
    return (PasswordChar01[y] >> 2) | GenByte00;
}

// $9011. Its three RORs leave bit 2 of the first character in the carry.
static uint8_t TwoLowerAndSixLower(uint8_t y, bool& carry) {
    uint8_t ch = PasswordChar00[y];
    carry = ch & 0x04;
    GenByte00 = (uint8_t)(ch << 6);
    return PasswordChar01[y] | GenByte00;
}

// $8F60: every four characters make three password bytes. Returns the carry it leaves, which reaches
// UnscramblePassword when NARPASSWORD is already active.
bool ConsolidatePassword() {
    bool carry = false;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t c = (uint8_t)(i * 4);
        uint8_t b = (uint8_t)(i * 3);
        PasswordByte00[b] = SixLowerAndTwoUpper(c);
        PasswordByte00[b + 1] = FourLoNFiveThruTwo(c + 1);
        PasswordByte00[b + 2] = TwoLowerAndSixLower(c + 2, carry);
    }
    return carry;
}

} // namespace Game
