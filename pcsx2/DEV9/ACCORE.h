#pragma once
#include "MemoryTypes.h"

/**
 * @brief source related to ACCORE.IRX MMIO and stuff
 * 
 */


namespace ACCORE {
    u16 Read16(u32 addr);
    void Write16(u32 addr, u16 val);
	void intr(int INTRN);
	void Interrupt(u32 mem, u16 val);
	
	enum {
		INTRN_ATA = 0x0,
		INTRN_JV = 0x1,
		INTRN_UART = 0x2,
		INTRN_LAST = 0x2,
	};
	enum INTC_CAUS {
		CAUS_ATA = 0x8000
	};
}

#define ACCORE_INTR_ATA  0x13000000
#define ACCORE_INTR_UART 0x13100000