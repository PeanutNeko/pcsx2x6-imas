
#include "common/Console.h"
#include "ACRAM.h"
#include "MemoryTypes.h"
#include "IopMem.h"

#define OOB_REPORT(T) Console.Error("%s: out of bound index: %08X", __FUNCTION__, T);
#define GET_RAM_OFF(t) ((t - ACRAM_ADDR_BASE)/2) // u8 buffer on u16 MMIO, halve the address to get real offset

u16 ACRAM::Read16(u32 addr) {
    u32 T = GET_RAM_OFF(addr);
    if (T < ACRAM_MAX_SIZE) {
        u8 A = iopMem->ACRAM[T];
        Console.WriteLn(Color_Blue, "%-16s %05X = %02X", __FUNCTION__, T, A);
        return A;
    } else OOB_REPORT(addr);
    return 0;
}

void ACRAM::Write16(u32 addr, u16 val) {
    u32 T = GET_RAM_OFF(addr);
    if (T < ACRAM_MAX_SIZE) {
        Console.WriteLn(Color_Blue, "%-16s %05X = %02X", __FUNCTION__, T, val);
        iopMem->ACRAM[T] = (u8)(val&0xFF);
    } else OOB_REPORT(addr);
}
