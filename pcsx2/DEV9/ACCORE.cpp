#include "DEV9.h"
#include "IopDma.h"
#include "ACCORE.h"
#include "ACJV.h"
#include "ACMACROS.h"
#include "common/Console.h"

#define ANS(addr, what) case addr: Console.Error("%-16s %08X: %04X", __FUNCTION__, addr, what); return what

u16 INTR_REG = 0;

u16 ACCORE::Read16(u32 mem) {
    switch (mem) {
	case 0x1241C000:
		// Console.Error("%-16s %08X: %04X", __FUNCTION__, mem, INTR_REG);
    	return INTR_REG; // ACRAM will wait 0xFFFFF times for this to not have 0x1000 bitmask set. also used inside intr_intr (interrup handler 13 declared on accore)
    
    break;
    default: Console.Error("%-16s %08X:  %04X", "ACUNK::Read16", mem, 0); return 0;
    }
    return 0;
}

void ACCORE::Write16(u32 mem, u16 value) {
		switch (mem) {
		case ACJV_CTR_START: Console.Warning("ACJV::START"); ACJV::enabled = true; break;
		case ACJV_CTR_STOP:  Console.Warning("ACJV::STOP");  ACJV::enabled = false;  break;
		case 0x1241510C:  Console.Warning("ACCORE::INTR  DISABLE_ACATA_INTR"); break;
		case 0x1241511C:  Console.Warning("ACCORE::INTR  DISABLE_ACUART_INTR"); break;
		// ACFPGA UPLOAD MMIO ///TODOx6: move this handling to ACFPGA.cpp
		case 0x12416012: // set to 0 after ACFPGALD uploads bitstream and during startup of ACRAM
		case 0x12416014:
		case 0x12416018:
		case 0x12416016:
		case 0x1241601A:
			break;
		// unknown addresses set to 0 on ACCORE. most likely stopping other stuff
		//case 0x12416032: break;
		//case 0x12416032: break;
		//case 0x12416036: break;
		//case 0x1241603A: break;
		//case 0x12417000: break;
		//case 0x1241601E: break;

		default: Console.Error("%-16s %08X = %04X", "ACUNK::write16", mem, value); break;
		}
}

void ACCORE::intr(int INTRN) {
	switch (INTRN)
	{
	case INTRN_ATA:
		INTR_REG |= CAUS_ATA;
		break;
	
	default:
		
		return;
	}
	//dev9Irq(1);
	//iopIntcIrq(13);
}

void ACCORE::Interrupt(u32 mem, u16 v) {
	switch (mem)
	{
	case ACCORE_INTR_ATA:
		Console.Warning("ACCORE:  ACATA_INTR_CLEAR: %04X", v);
		CLRB(INTR_REG, CAUS_ATA);
		break;
	case ACCORE_INTR_UART:
		Console.Warning("ACCORE: ACUART_INTR_CLEAR: %04X", v);
		break;
	default:
		Console.Warning("ACCORE: unknown INTR write to: %08X", mem);
		break;
	}
}