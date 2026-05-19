#pragma once

#include "common/Pcsx2Types.h"
#include "common/Pcsx2Defs.h"

/**
 * @file ACRAM.h
 * Additional RAM memory living on the namco board
 * The RAM can be 32 or 64mb sized, or directly, not be there
 * System246 DRIVING : ACRAM is an external PCB, can be 0, 32, 64
 * System246 Rack A : Same than DRIVING unit
 * System246 Rack B : Builtin on the namco board, seems like 64mb version
 * System246 Rack C : same than Rack B
 * System256 : ACRAM is no longer part of the system
 */

#define ACRAM_ADDR_BASE 0xB4000000
#define ACRAM_RANGE 0x1400
#define ACRAM_ADDR_BASE_IOP_POV 0x14000000
#define ACRAM_MAX_SIZE _64mb

namespace ACRAM
{
    u16 Read16(u32 addr);
    void Write16(u32 addr, u16 val);
}

