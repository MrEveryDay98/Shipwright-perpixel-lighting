// Bank 7: room setup and drawing ($EA05-$F1A6). A room is drawn into room RAM ($6000 for name table 0,
// $6400 for name table 3) one structure at a time; structures are rows of 2x2 tile macros. The room
// data then lists enemies, doors, elevators and statues, and the area's special item list adds
// power-ups, Mother Brain, zebetites and the like. Also the collision checks between Samus, her
// projectiles, enemies and Memus ($F034).

#include "game/Room.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Items.h"
#include "game/Memory.h"
#include "game/Original.h"
#include "game/Present.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;
using namespace Const;

// --- $EA05 SelectRoomRAM -----------------------------------------------------------------------------

// Room RAM for the name table the room goes on: $6000 (name table 0) or $6400 (name table 3).
void SelectRoomRAM() {
    CartRAMPtrUB = (uint8_t)((GetNameTable() << 2) | 0x60);
    CartRAMPtrLB = 0x00;
}

// --- $EA13 AttribTableWrite --------------------------------------------------------------------------

// Once the room is drawn, RoomNumber counts $F0-$F4: the attribute table goes to the PPU in two halves,
// then the room is finished.
void AttribTableWrite() {
    static const uint16_t kAttribTableRoutines = 0xEA1C;
    uint8_t index = RoomNumber & 0x0F;
    RoomNumber++;
    ChooseRoutineEffects(kAttribTableRoutines, index);
    switch (index) {
        case 1:
        case 3:
            WritePPUAttribTbl();
            break;
        case 4:
            RoomFinished();
            break;
        default:
            break; // ExitSub
    }
}

// $EA26
void RoomFinished() {
    RoomNumber = 0xFF;
}

// --- $EA2B SetupRoom ---------------------------------------------------------------------------------

// RoomNumber $FF: nothing to do; $FE: continue drawing the room; $F0-$F4: write the attribute table;
// otherwise start a new room.
void SetupRoom() {
    uint8_t room = RoomNumber;
    if (room == 0xFF) {
        return;
    }
    if (room == 0xFE) {
        DrawRoom();
        return;
    }
    if (room >= 0xF0) {
        AttribTableWrite();
        return;
    }
    uint8_t x = UpdateRoomSpriteInfo();
    ScanForItems(x);
    uint8_t y = (uint8_t)(RoomNumber << 1);
    uint16_t table = RoomPtrTable.Word();
    RoomPtrLB = Read((uint16_t)(table + y));
    y++;
    RoomPtrUB = Read((uint16_t)(table + y));
    RoomPal = Read(RoomPtr.Word()); // palette the attribute table starts filled with
    AddToRoomPtr(0x01);
    SelectRoomRAM();
    Present::OnRoomDrawn();
    InitTables();
    DrawRoom();
}

// --- $EA60 DrawObject --------------------------------------------------------------------------------

// Draws the room object at RoomPtr: position (%yyyyxxxx, in macros), structure number and palette.
// The original then falls into DrawRoom for the next object; DrawRoom loops instead.
void DrawObject(uint8_t pos) {
    Mem(0x0E) = pos;
    CartRAMWorkPtrLB = CartRAMPtrLB;
    CartRAMWorkPtrUB = CartRAMPtrUB;
    bool carry;
    for (uint8_t x = pos >> 4; x != 0; x--) {
        carry = false;
        CartRAMWorkPtrLB = Adc(CartRAMWorkPtrLB, 0x40, carry); // two tile rows
        if (carry) {
            CartRAMWorkPtrUB++;
        }
    }
    carry = false;
    CartRAMWorkPtrLB = Adc((uint8_t)((pos & 0x0F) << 1), CartRAMWorkPtrLB, carry);
    if (carry) {
        CartRAMWorkPtrUB++;
    }
    uint16_t room = RoomPtr.Word();
    uint8_t structure = Read((uint16_t)(room + 1));
    ObjectPal = Read((uint16_t)(room + 2));
    uint8_t y = (uint8_t)(structure << 1);
    uint16_t table = StructPtrTable.Word();
    StructPtrLB = Read((uint16_t)(table + y));
    y++;
    StructPtrUB = Read((uint16_t)(table + y));
    DrawStruct();
    AddToRoomPtr(0x03);
}

// --- $EAAA DrawRoom ----------------------------------------------------------------------------------

// Draws room objects until the end of the room ($FF), a pause until next frame ($FE) or the end of the
// objects ($FD, enemies and doors follow).
void DrawRoom() {
    for (;;) {
        uint8_t a = Read(RoomPtr.Word());
        if (a == 0xFF) {
            EndOfRoom();
            return;
        }
        if (a == 0xFE) {
            RoomNumber = a;
            AddToRoomPtr(0x01);
            return;
        }
        if (a == 0xFD) {
            EndOfObjs();
            return;
        }
        DrawObject(a);
    }
}

// $EAC0
void AddToRoomPtr(uint8_t a) {
    bool carry = false;
    RoomPtrLB = Adc(a, RoomPtrLB, carry);
    if (carry) {
        RoomPtrUB++;
    }
}

// --- $EACA EndOfObjs ---------------------------------------------------------------------------------

// Loads the room's enemies, doors, elevator, statues and Zeb holes.
void EndOfObjs() {
    static const uint16_t kRoomItemRoutines = 0xEAE4;
    Mem(0x00) = RoomPtrLB;
    Mem(0x01) = RoomPtrUB;
    uint8_t a = 0x01;
    uint8_t x = 0; // only written by the loaders
    for (;;) {
        // EnemyLoop
        AddToPtr00(a);
        uint8_t data = Read(GenPtr00.Word());
        if (data == 0xFF) {
            EndOfRoom();
            return;
        }
        uint8_t index = data & 0x0F;
        ChooseRoutineEffects(kRoomItemRoutines, index);
        switch (index) {
            case 1: // LoadEnemy
                a = GetEnemyData(x);
                break;
            case 2: // LoadDoor
                a = LoadDoorData(x);
                break;
            case 4: // LoadElevator
                a = LoadElevatorData();
                break;
            case 6:
                a = LoadStatues();
                break;
            case 7:
                a = ZebHole();
                break;
            default:
                return; // ExitSub: returns from SetupRoom without finishing the room
        }
    }
}

// --- $EAF4 EndOfRoom ---------------------------------------------------------------------------------

// The room is in room RAM: the attribute table follows ($F0), and the first row or column is written to
// the name table ($E57C / $E70C, entered with TempScrollDir equal to ScrollDir).
void EndOfRoom() {
    RoomNumber = 0xF0;
    TempScrollDir = ScrollDir;
    if (!(ScrollDir & 0x02)) {
        // $E57C: $01:$00 = (ScrollY & $F8) * 4
        Mem(0x00) = ScrollY & 0xF8;
        uint8_t a = 0;
        for (int i = 0; i < 2; i++) {
            bool carry = false;
            Mem(0x00) = Asl(Mem(0x00), carry);
            a = Rol(a, carry);
        }
        WriteNameTableLine(a);
    } else {
        // $E70C
        Mem(0x00) = (uint8_t)((ScrollX & 0xF8) >> 3);
        WriteNameTableLine(0x00);
    }
}

// --- $EB0C GetEnemyData ------------------------------------------------------------------------------

// Loads the enemy at ($00): slot and type, then type, then position. Returns the size of the entry; x
// is the enemy slot.
uint8_t GetEnemyData(uint8_t& x) {
    uint16_t data = GenPtr00.Word();
    x = Read(data) & 0xF0;
    if (IsSlotTaken(x) != 0) {
        return 0x03;
    }
    if (GetEnemyType(x, Read((uint16_t)(data + 1)))) {
        SetEnemyPosition(x, Read((uint16_t)(data + 2)));
    }
    return 0x03;
}

// $EB28: returns false for Kraid or Ridley once they are dead: the original drops GetEnemyData's return
// address and returns from it ($EB3C), so the enemy isn't placed.
bool GetEnemyType(uint8_t x, uint8_t type) {
    EnSpecialAttribs[x] = type & 0xC0; // bit 7: tough version, bit 6: mini boss
    if (type & 0x40) {
        uint8_t y = (InArea & 0x06) >> 1; // Kraid's or Ridley's statue status
        if (MaxMissiles[y] != 0) {
            return false;
        }
        KrdRdlyPresent = 0x01;
    }
    EnDataIndex[x] = type & 0x3F;
    return true;
}

// $EB4D: position (%yyyyxxxx) and initial data of the enemy in slot x.
void SetEnemyPosition(uint8_t x, uint8_t pos) {
    EnYRoomPos[x] = (uint8_t)((pos & 0xF0) | 0x08);
    EnXRoomPos[x] = (uint8_t)((pos << 4) | 0x0C);
    Enstatus[x] = 0x01;
    Mem((uint16_t)(0x0404 + x)) = 0x00;
    uint8_t a = GetNameTable();
    EnNameTable[x] = a;
    uint8_t y = EnDataIndex[x];
    bool carry;
    Mem((uint16_t)(0x0405 + x)) = Asl(Mem((uint16_t)(0x0405 + x)), carry);
    Regs r = Original(0xFB7B, { a, x, y, carry });
    Original(0xF85A, r);
}

// $EB7A: non-zero when slot x holds an enemy that stays.
uint8_t IsSlotTaken(uint8_t x) {
    if (Enstatus[x] == 0) {
        return 0;
    }
    return Mem((uint16_t)(0x0405 + x)) & 0x02;
}

// --- $EB92 LoadDoor ----------------------------------------------------------------------------------

// Loads the door at ($00): side and palette. Red doors already opened (in the item history) stay open.
// Returns the size of the entry; x is left at the name table the door is on, flipped.
uint8_t LoadDoorData(uint8_t& x) {
    static const uint16_t kDoorXs = 0xEBFC;
    static const uint16_t kDoorBits = 0xEBFE;
    static const uint16_t kDoorSlots = 0xEC00;
    uint8_t info = Read((uint16_t)(GenPtr00.Word() + 1));
    bool side = info & 0x10; // carry out of Amul16: 0 right, 1 left
    bool carry = side;
    uint8_t y = Rol((uint8_t)(MapPosX + MapPosY), carry) & 0x03;
    x = Read((uint16_t)(kDoorSlots + y));
    Mem((uint16_t)(0x0307 + x)) = info & 0x03; // door palette
    uint8_t saved = y;
    uint8_t pal = Mem((uint16_t)(0x0307 + x));
    bool present = true;
    if (pal != 0x01 && pal != 0x03) {
        Mem(0x09) = 0x0A;
        uint8_t mapX = MapPosX;
        if (x & 0x10) {
            mapX--;
        }
        SetItemID(mapX);
        present = !CheckForItem();
    }
    if (present) {
        ObjAction[x] = 0x01;
    }
    y = saved & 0x01;
    ObjectHi[x] = GetNameTable();
    ObjectX[x] = Read((uint16_t)(kDoorXs + y));
    ObjectY[x] = 0x68;
    y = Read((uint16_t)(kDoorBits + y));
    x = GetNameTable() ^ 0x01;
    DoorOnNameTable3[x] |= y;
    return 0x02;
}

// --- $EC09 LoadElevator ------------------------------------------------------------------------------

uint8_t LoadElevatorData() {
    if (ElevatorStatus != 0) {
        return 0x02;
    }
    Mem(0x032F) = Read((uint16_t)(GenPtr00.Word() + 1));
    Mem(0x032D) = 0x83; // Y
    Mem(0x032E) = 0x80; // X
    Mem(0x032C) = GetNameTable();
    Mem(0x0323) = 0x23; // frame
    ElevatorStatus++;
    return 0x02;
}

// --- $EC2F LoadStatues -------------------------------------------------------------------------------

uint8_t LoadStatues() {
    Mem(0x036C) = GetNameTable();
    Mem(0x0370) = (RidlyStatueStat & 0x80) ? 0x30 : 0x40; // raised when hit
    Mem(0x036F) = (KraidStatueStat & 0x80) ? 0x50 : 0x60;
    Mem(0x54) = 0x00; // Y is 0 here
    Mem(0x0360) = 0x01;
    return 0x01;
}

// --- $EC57 ZebHole -----------------------------------------------------------------------------------

// A pipe that regenerates enemies, in the first free of four slots at $0728.
uint8_t ZebHole() {
    uint8_t x = 0x20;
    for (;;) {
        bool carry = true;
        uint8_t a = Sbc(x, 0x08, carry);
        if (a & 0x80) {
            return 0x03; // no free slot
        }
        x = a;
        if (Mem((uint16_t)(0x0728 + x)) == 0xFF) {
            break;
        }
    }
    uint16_t data = GenPtr00.Word();
    Mem((uint16_t)(0x0729 + x)) = Read(data) & 0xF0;
    Mem((uint16_t)(0x0728 + x)) = Read((uint16_t)(data + 1));
    uint8_t pos = Read((uint16_t)(data + 2));
    Mem((uint16_t)(0x072A + x)) = (uint8_t)((pos & 0xF0) | 0x08);
    Mem((uint16_t)(0x072B + x)) = (uint8_t)(pos << 4);
    Mem((uint16_t)(0x072C + x)) = GetNameTable();
    return 0x03;
}

// --- $EC9B UpdateRoomSpriteInfo ----------------------------------------------------------------------

// Before a new room is loaded on a name table, removes what is still there from the room before:
// enemies, Memus, projectiles, tiles, the elevator, statues, Zeb holes and power-ups. Returns X as the
// area routine at $95AE leaves it.
uint8_t UpdateRoomSpriteInfo() {
    uint8_t x = (uint8_t)(ScrollDir - 1);
    UpdateDoorData(x, 0);
    UpdateDoorData(x, 1);
    uint8_t y = GetNameTable();
    x = 0x50;
    do {
        if (!((y ^ EnNameTable[x]) & 0x01)) {
            uint8_t a = Mem((uint16_t)(0x0405 + x)) & 0x02;
            if (a == 0) {
                Enstatus[x] = a;
            }
        }
        x = (uint8_t)(x - 0x10);
    } while (!(x & 0x80));
    x = 0x18;
    do {
        if (!((y ^ Mem((uint16_t)(0xB3 + x))) & 0x01)) {
            Mem((uint16_t)(0xB0 + x)) = 0x00;
        }
        x = (uint8_t)(x - 0x08);
    } while (!(x & 0x80));
    ClearOffScreenObjects();
    ClearDoorDataOtherTable();
    y = (uint8_t)(GetNameTable() << 2);
    x = 0xC0;
    do {
        uint8_t a = (y ^ TileWRAMHi[x]) & 0x04;
        if (a == 0) {
            TileRoutine[x] = a;
        }
        x = (uint8_t)(x - 0x10);
    } while (x != 0xF0);
    y >>= 2;
    ClearProjectileOnTable(0xD0, y);
    ClearProjectileOnTable(0xE0, y);
    ClearProjectileOnTable(0xF0, y);
    uint8_t a = (uint8_t)(y - Mem(0x032C));
    if (a == 0) {
        ElevatorStatus = a;
    }
    x = 0x1E;
    do {
        if (Mem((uint16_t)(0x0704 + x)) == 0) {
            Mem((uint16_t)(0x0700 + x)) = 0xFF;
        }
        x = (uint8_t)(x - 0x06);
    } while (!(x & 0x80));
    if (y == Mem(0x036C)) {
        Mem(0x0360) = 0x00; // statues
    }
    x = 0x18;
    do {
        if (y == Mem((uint16_t)(0x072C + x))) {
            Mem((uint16_t)(0x0728 + x)) = 0xFF; // Zeb hole
        }
        x = (uint8_t)(x - 0x08);
    } while (!(x & 0x80));
    ClearPowerUpOnTable(0x00, y);
    ClearPowerUpOnTable(0x08, y);
    a = y == PowerUpBNameTbl ? 0xFF : y;
    return AreaHook(0x95AE, { a, 0x08, y }).x;
}

// $ED51: moves the door data of name table y when the room changes name tables.
void UpdateDoorData(uint8_t x, uint8_t y) {
    Mem((uint16_t)(0x6C + y)) = (x ^ 0x03) & Mem((uint16_t)(0x6C + y));
}

// $ED5B
void ClearDoorDataOtherTable() {
    uint8_t y = GetNameTable() ^ 0x01;
    Mem((uint16_t)(0x6C + y)) = 0x00;
}

// $ED65: removes the doors and other objects ($80-$B0) that are off screen.
void ClearOffScreenObjects() {
    uint8_t x = 0xB0;
    do {
        if (ObjAction[x] != 0 && ObjectOnScreen[x] == 0) {
            ObjAction[x] = 0x00;
        }
        x = (uint8_t)(x - 0x10);
    } while (x & 0x80);
}

// $ED7A: removes projectile x if it is on name table y (bombs and explosions only).
void ClearProjectileOnTable(uint8_t x, uint8_t y) {
    if (ObjAction[x] < 0x05) {
        return;
    }
    uint8_t a = y ^ ObjectHi[x];
    if (a & 0x01) {
        return;
    }
    ObjAction[x] = a >> 1;
}

// $ED8C
void ClearPowerUpOnTable(uint8_t x, uint8_t y) {
    if (y == PowerUpNameTable[x]) {
        PowerUpType[x] = 0xFF;
    }
}

// --- $ED98 ScanForItems ------------------------------------------------------------------------------

// The area's special item list is sorted by map Y, then map X. Finds the entries at Samus's map position
// and runs their handlers. x is X at entry (some area routines are entered with it).
void ScanForItems(uint8_t x) {
    static const uint16_t kSpecItmsTable = 0x9598;
    static const uint16_t kHandlerRoutines = 0xEDE2;
    Mem(0x00) = Read(kSpecItmsTable);
    uint8_t a = Read(kSpecItmsTable + 1);
    for (;;) {
        // ScanOneItem
        Mem(0x01) = a;
        uint16_t item = GenPtr00.Word();
        uint8_t mapY = Read(item);
        if (mapY == MapPosY) {
            break;
        }
        if (mapY > MapPosY) {
            return;
        }
        x = Read((uint16_t)(item + 1));
        if ((x & Read((uint16_t)(item + 2))) == 0xFF) {
            return; // last item
        }
        a = Read((uint16_t)(item + 2));
        Mem(0x00) = x;
    }
    AddToPtr00(0x03);
    for (;;) {
        // ScanItemX
        uint8_t mapX = Read(GenPtr00.Word());
        if (mapX == MapPosX) {
            break;
        }
        if (mapX > MapPosX) {
            return;
        }
        if (!AnotherItem(0x01)) {
            return; // the original pops ScanItemX's return address: returns from ScanForItems
        }
    }
    a = 0x02;
    for (;;) {
        // ChooseHandlerRoutine
        AddToPtr00(a);
        uint8_t index = Read(GenPtr00.Word()) & 0x0F;
        ChooseRoutineEffects(kHandlerRoutines, index);
        switch (index) {
            case 1: // SqueeptHandler
                a = GetEnemyData(x);
                break;
            case 2:
                a = PowerUpHandler(x);
                break;
            case 3:
                a = SpecEnemyHandler(x);
                break;
            case 4: // ElevatorHandler
                a = LoadElevatorData();
                break;
            case 5:
                a = CannonHandler(x);
                break;
            case 6:
                a = MotherBrainHandler(x);
                break;
            case 7:
                a = ZeebetiteHandler(x);
                break;
            case 8:
                a = RinkaHandler(x);
                break;
            case 9: // DoorHandler
                a = LoadDoorData(x);
                break;
            case 10: // PaletteHandler ($EEFA)
                Mem(0x91) = ScrollDir;
                if (ScrollDir != 0) {
                    a = 0x01;
                    break;
                }
                // Falls into AnotherItem with Y = 0: the entry's own type byte ($xA, never $FF) is added to
                // the pointer and ScanForItems returns.
                AnotherItem(0x00);
                return;
            default:
                return; // ExitSub
        }
    }
}

// $EDFE: places a power-up in a free item slot unless Samus already has it. x is the slot (0 or 8).
uint8_t PowerUpHandler(uint8_t& x) {
    uint16_t data = GenPtr00.Word();
    x = 0x00;
    if (PowerUpType != 0xFF) {
        x = 0x08;
        if (PowerUpBType != 0xFF) {
            return 0x03;
        }
    }
    PrepareItemID(Read((uint16_t)(data + 1)));
    if (CheckForItem()) {
        return 0x03;
    }
    PowerUpType[x] = Mem(0x09);
    uint8_t pos = Read((uint16_t)(data + 2));
    PowerUpYCoord[x] = (uint8_t)((pos & 0xF0) | 0x08);
    PowerUpXCoord[x] = (uint8_t)((pos << 4) | 0x08);
    PowerUpNameTable[x] = GetNameTable();
    return 0x03;
}

// $EE3D
void PrepareItemID(uint8_t type) {
    Mem(0x09) = type;
    SetItemID(MapPosX);
}

// $EE41: the unique item ID ($06/$07) of item type $09 at map position (mapX, MapPosY).
void SetItemID(uint8_t mapX) {
    Mem(0x07) = mapX;
    Mem(0x06) = MapPosY;
    CreateItemID();
}

// $EE4A: the carry: true when $06/$07 is in the unique item history.
bool CheckForItem() {
    uint8_t y = NumUniqueItems;
    if (y == 0) {
        return false;
    }
    do {
        if (Mem(0x07) == NumUniqueItems[y] && Mem(0x06) == DataSlot[y]) {
            return true;
        }
        y = (uint8_t)(y - 2);
    } while (y != 0);
    return false;
}

// $EE63: Memus (and the like) in their four slots at $B0, with random positions.
uint8_t SpecEnemyHandler(uint8_t& x) {
    x = 0x18;
    bool carry = false; // from ChooseRoutine's ASL
    Mem(0x8A) = Adc(RandomNumber1, FrameCount, carry);
    for (;;) {
        InitMemu(x, carry);
        carry = true;
        uint8_t a = Sbc(x, 0x08, carry);
        x = a;
        if (a & 0x80) {
            break;
        }
    }
    uint8_t a = Read(0x95E4);
    Mem(0x6BE9) = a;
    Mem(0x6BEA) = a;
    Mem(0x6BE4) = 0x01;
    return 0x01;
}

// $EE86
void InitMemu(uint8_t x, bool& carry) {
    if (Mem((uint16_t)(0xB0 + x)) != 0) {
        return;
    }
    uint8_t a = Adc(x, Mem(0x8A), carry) & 0x7F;
    Mem((uint16_t)(0xB1 + x)) = a;
    Mem((uint16_t)(0xB2 + x)) = Adc(a, RandomNumber2, carry);
    Mem((uint16_t)(0xB3 + x)) = GetNameTable();
    Mem((uint16_t)(0xB0 + x)) = 0x01;
    Mem(0x8A) = Rol(Mem(0x8A), carry);
}

// $EEA6: Mother Brain's room cannons.
uint8_t CannonHandler(uint8_t& x) {
    x = AreaHook(0x95B1, { 0, x, 0 }).x;
    return 0x02;
}

// $EEAE: Mother Brain stays dead once she is in the item history.
uint8_t MotherBrainHandler(uint8_t& x) {
    x = AreaHook(0x95B4, { 0, x, 0 }).x;
    Mem(0x07) = 0x38;
    Mem(0x06) = 0x00;
    if (CheckForItem()) {
        MthrBrainStatus = 0x08;
        MotherBrainHits = 0x00;
    }
    return 0x01;
}

// $EECA: zebetites already destroyed stay destroyed.
uint8_t ZeebetiteHandler(uint8_t& x) {
    x = AreaHook(0x95B7, { 0, x, 0 }).x;
    bool carry;
    uint8_t a = Lsr(x, carry);
    Mem(0x07) = Adc(a, 0x3C, carry);
    Mem(0x06) = 0x00;
    if (CheckForItem()) {
        Mem((uint16_t)(0x0758 + x)) = 0x81;
        Mem((uint16_t)(0x075D + x)) = 0x01;
        Mem((uint16_t)(0x075B + x)) = 0x07;
    }
    return 0x01;
}

// $EEEE
uint8_t RinkaHandler(uint8_t& x) {
    x = AreaHook(0x95BA, { 0, x, 0 }).x;
    return 0x01;
}

// $EF00: false when there is no other item at the same map Y. The original then pops its caller's
// return address and returns from ScanForItems.
bool AnotherItem(uint8_t y) {
    uint8_t a = Read((uint16_t)(GenPtr00.Word() + y));
    if (a == 0xFF) {
        return false;
    }
    AddToPtr00(a);
    return true;
}

// $EF09
void AddToPtr00(uint8_t a) {
    bool carry = false;
    Mem(0x00) = Adc(a, Mem(0x00), carry);
    if (carry) {
        Mem(0x01)++;
    }
}

// --- $EF13 DrawStructRow -----------------------------------------------------------------------------

// Draws one row of macros of the structure (low nibble: count, 0 = 16; high nibble: x offset). Returns
// false when the room RAM is full (the original returns from DrawStruct).
bool DrawStructRow(uint8_t a) {
    static const uint16_t kTilePosTable = 0xEF9A;
    a &= 0x0F;
    if (a == 0) {
        a = 0x10;
    }
    Mem(0x0E) = a;
    uint8_t offset = (uint8_t)((Read(StructPtr.Word()) >> 4) << 1);
    bool carry = false;
    Mem(0x00) = Adc(offset, CartRAMWorkPtrLB, carry);
    Mem(0x01) = Adc(0x00, CartRAMWorkPtrUB, carry);
    for (;;) {
        // DrawMacro
        uint8_t hi = Mem(0x01);
        if (hi == 0x63 || hi == 0x67) {
            if (Mem(0x00) >= 0xA0) {
                return false; // reached the attribute table
            }
        } else if (hi > 0x67) {
            return false;
        }
        Mem(0x10)++;
        Mem(0x11) = (uint8_t)(Read((uint16_t)(StructPtr.Word() + Mem(0x10))) << 2);
        for (int x = 3; x >= 0; x--) {
            uint8_t tile = Read((uint16_t)(MacroPtr.Word() + Mem(0x11)));
            Mem(0x11)++;
            uint8_t y = Read((uint16_t)(kTilePosTable + x));
            Write((uint16_t)(GenPtr00.Word() + y), tile);
        }
        UpdateAttrib();
        AddYToPtr00(0x02);
        if ((Mem(0x00) & 0x1F) == 0) {
            // End of the room RAM row: skip the rest of the structure row.
            AdvanceRow((uint8_t)(Mem(0x10) + Mem(0x0E) - 1));
            return true;
        }
        if (--Mem(0x0E) == 0) {
            AdvanceRow(Mem(0x10));
            return true;
        }
    }
}

// $EF78: moves the structure pointer past the row (a + 1 bytes) and the room RAM pointer two tile rows
// down.
void AdvanceRow(uint8_t a) {
    bool carry = true;
    StructPtrLB = Adc(a, StructPtrLB, carry);
    if (carry) {
        StructPtrUB++;
    }
    carry = false;
    CartRAMWorkPtrLB = Adc(0x40, CartRAMWorkPtrLB, carry);
    if (carry) {
        CartRAMWorkPtrUB++;
    }
}

// $EF8C
void DrawStruct() {
    for (;;) {
        Mem(0x10) = 0x00;
        uint8_t a = Read(StructPtr.Word());
        if (a == 0xFF) {
            return;
        }
        if (!DrawStructRow(a)) {
            return;
        }
    }
}

// --- $EF9E UpdateAttrib ------------------------------------------------------------------------------

// Sets the attribute bits of the macro just drawn at ($00) to ObjectPal.
void UpdateAttrib() {
    static const uint16_t kAttribMaskTable = 0xEFF4;
    if (ObjectPal == RoomPal) {
        return;
    }
    Mem(0x02) = Mem(0x00);
    bool carry;
    uint8_t a = Lsr(Mem(0x01), carry);
    Mem(0x02) = Ror(Mem(0x02), carry);
    Lsr(a, carry);
    Mem(0x02) = Ror(Mem(0x02), carry);
    Mem(0x03) = Mem(0x02) & 0x07;
    Mem(0x02) = (uint8_t)(((Mem(0x02) >> 2) & 0x38) | Mem(0x03) | 0xC0);
    Mem(0x03) = 0x63;
    uint8_t x = (Mem(0x00) & 0x40) ? 0x02 : 0x00;
    if (Mem(0x00) & 0x02) {
        x++;
    }
    Mem(0x03) = (Mem(0x01) & 0x04) | Mem(0x03);
    uint16_t attrib = GenPtr02.Word();
    Write(attrib, Read((uint16_t)(kAttribMaskTable + x)) & Read(attrib));
    a = ObjectPal;
    for (;;) {
        x--;
        if (x & 0x80) {
            break;
        }
        a = Asl(a, carry);
        a = Asl(a, carry);
        if (carry) {
            break;
        }
    }
    Write(attrib, a | Read(attrib));
}

// --- $EFF8 InitTables --------------------------------------------------------------------------------

// Fills the room RAM with blank tiles and its attribute table with the room's palette.
void InitTables() {
    static const uint16_t kATDataTable = 0xF018;
    uint8_t hi = CartRAMPtrUB;
    FillRoomRAM(0xFF, hi, (uint8_t)(hi + 3));
    Mem(0x01) = (uint8_t)(Mem(0x01) + 4);
    uint8_t a = Read((uint16_t)(kATDataTable + RoomPal));
    for (uint8_t y = 0xC0; y != 0; y++) {
        Write((uint16_t)(GenPtr00.Word() + y), a);
    }
}

// $F01C: fills pages y down to x with a ($01 is left one page below).
void FillRoomRAM(uint8_t a, uint8_t x, uint8_t y) {
    Mem(0x01) = y;
    bool carry = false;
    x = Sbc(x, Mem(0x01), carry);
    y = 0;
    Mem(0x00) = y;
    do {
        do {
            Write((uint16_t)(GenPtr00.Word() + y), a);
            y--;
        } while (y != 0);
        Mem(0x01)--;
        x++;
    } while (x != 0);
}

// --- $F034 Collisions ---------------------------------------------------------------------------------

// Checks Memus against Samus and her projectiles, the doors against Samus, enemies against projectiles
// and Samus, the other objects at $60-$B0 against Samus and bombs/explosions against Samus, then
// applies the damage. The distance and hit routines from $F1A7 on aren't native yet; they work on the
// registers and the carry passed along here.
void SamusEnemyCollisions() {
    Mem(0x73) = 0xFF;
    Mem(0x010F) = 0xFF;
    Regs r;
    auto isHittingProjectile = [](uint8_t action) {
        return action < wa_BulletExplode || action == 0x07 || action == wa_BombExplode || action == wa_Missile;
    };
    // Memus.
    uint8_t x = 0x18;
    uint8_t y = 0;
    do {
        uint8_t status = Mem((uint16_t)(0xB0 + x));
        if (status != 0 && status != 0x03) {
            GetMemu0CoordData(x);
            if (!IsSamusDead() && SamusBlink == 0) {
                r = MemuCollision(x, 0x00);
                r = Original(0xF2B4, r);
                x = r.x;
            }
            for (y = 0xD0; y != 0; y = (uint8_t)(y + 0x10)) {
                uint8_t action = ObjAction[y];
                if (action != 0 && isHittingProjectile(action)) {
                    r = MemuCollision(x, y);
                    r = Original(0xF32A, r);
                    x = r.x;
                    y = r.y;
                }
            }
        }
        x = (uint8_t)(x - 0x08);
    } while (!(x & 0x80));
    // Doors.
    x = 0xB0;
    do {
        if (ObjAction[x] == 0x02) {
            y = 0x00;
            if (IsSamusDead()) {
                break; // on to the enemies
            }
            r = Original(0xDC7F, { 0, x, y }); // AreObjectsTouching
            r = Original(0xF277, r);
            x = r.x;
        }
        x = (uint8_t)(x - 0x10);
    } while (x & 0x80);
    // Enemies.
    x = 0x50;
    do {
        uint8_t status = Enstatus[x];
        if (status != 0 && status != 0x03) {
            GetEnemy0CoordData(x);
            if (Enstatus[x] != 0x05) {
                for (y = 0xD0; y != 0; y = (uint8_t)(y + 0x10)) {
                    uint8_t action = ObjAction[y];
                    if (action != 0 && isHittingProjectile(action)) {
                        r = EnemyCollision(x, y);
                        r = Original(0xF2CA, r);
                        x = r.x;
                        y = r.y;
                    }
                }
            }
            y = 0x00;
            if (SamusBlink == 0 && !IsSamusDead()) {
                r = EnemyCollision(x, y);
                r = Original(0xF282, r);
                x = r.x;
            }
        }
        x = (uint8_t)(x - 0x10);
    } while (!(x & 0x80));
    // Samus against the objects at $60-$B0.
    x = 0x00;
    GetObject0CoordData(x);
    for (y = 0x60; y != 0xC0; y = (uint8_t)(y + 0x10)) {
        uint8_t status = Enstatus[y];
        if (status != 0 && status != 0x05 && SamusBlink == 0 && !IsSamusDead()) {
            r = Original(0xF1B3, { 0, x, y }); // DistFromObj0ToEn1
            GetEnemy1CoordData(r.y);
            r = Original(0xF1FA, { 0, r.x, r.y });
            r = Original(0xF2ED, r);
            x = r.x;
            y = r.y;
        }
    }
    // Samus against bombs and explosions.
    y = 0x00;
    if (!IsSamusDead()) {
        GetObject1CoordData(y);
        for (x = 0xF0; x != 0xC0; x = (uint8_t)(x - 0x10)) {
            uint8_t action = ObjAction[x];
            if (action == 0x07 || action == 0x0A) {
                r = Original(0xDC82, { 0, x, y });
                r = Original(0xF311, r);
                x = r.x;
                y = r.y;
            }
        }
    }
    SubtractHealth();
}

// $F140: distance check between enemy x and object y. Returns the registers and carry of $F1FA.
Regs EnemyCollision(uint8_t x, uint8_t y) {
    Regs r = Original(0xF1BF, { 0, x, y }); // DistFromEn0ToObj1
    GetObject1CoordData(r.y);
    return Original(0xF1FA, { 0, r.x, r.y });
}

// $F149: distance check between a Memu (coordinates already in $07/$09/$0B) and object y.
Regs MemuCollision(uint8_t x, uint8_t y) {
    GetObject1CoordData(y);
    Regs r = Original(0xF1D2, { 0, x, y });
    return Original(0xF1FA, { 0, r.x, r.y });
}

// $F152
void GetEnemy0CoordData(uint8_t x) {
    Mem(0x07) = EnYRoomPos[x];
    Mem(0x09) = EnXRoomPos[x];
    SetObject0NameTable(EnNameTable[x]);
}

// $F162
void GetEnemy1CoordData(uint8_t y) {
    Mem(0x06) = EnYRoomPos[y];
    Mem(0x08) = EnXRoomPos[y];
    SetObject1NameTable(EnNameTable[y]);
}

// $F172
void GetObject0CoordData(uint8_t x) {
    Mem(0x07) = ObjectY[x];
    Mem(0x09) = ObjectX[x];
    SetObject0NameTable(ObjectHi[x]);
}

// $F17F
void SetObject0NameTable(uint8_t hi) {
    Mem(0x0B) = (hi ^ PPUCNT0ZP) & 0x01;
}

// $F186
void GetObject1CoordData(uint8_t y) {
    Mem(0x06) = ObjectY[y];
    Mem(0x08) = ObjectX[y];
    SetObject1NameTable(ObjectHi[y]);
}

// $F193
void SetObject1NameTable(uint8_t hi) {
    Mem(0x0A) = (hi ^ PPUCNT0ZP) & 0x01;
}

// $F19A
void GetMemu0CoordData(uint8_t x) {
    Mem(0x07) = Mem((uint16_t)(0xB1 + x));
    Mem(0x09) = Mem((uint16_t)(0xB2 + x));
    SetObject0NameTable(Mem((uint16_t)(0xB3 + x)));
}

} // namespace Game
