#include "ACUART.h"
#include "ACJV.h"
#include "ACCORE.h"
#include "common/Console.h"
#include <algorithm>
#include <deque>
#include <string>
#include <vector>
#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

// 16550 UART register emulation for Namco System 246 arcade I/O
// Ref: ps2sdk iop/arcade/acuart/src/uart.c
// Register map (16-bit access, 2-byte stride from base 0x12418000):
//   +0x00  THR/RBR/DLL  (TX/RX data or divisor latch low when DLAB=1)
//   +0x02  IER/DLH      (interrupt enable or divisor latch high when DLAB=1)
//   +0x04  IIR/FCR      (interrupt ID read / FIFO control write)
//   +0x06  LCR          (line control, bit 7 = DLAB)
//   +0x08  MCR          (modem control)
//   +0x0A  LSR          (line status)
//   +0x0C  MSR          (modem status)
//   +0x0E  SCR          (scratch)

static u16 uart_ier = 0;
static u16 uart_lcr = 0;
static u16 uart_mcr = 0;
static u16 uart_scr = 0;
static u16 uart_dll = 0;
static u16 uart_dlh = 0;
static u16 uart_fcr_shadow = 0;

// State for Battle Gear 3's boot handshake with its steering ("HANDLE") board; cleared each boot by ResetBg3State.
static int s_bg3TxCnt = 0;
static u8 s_bg3PrevTx = 0;
static int s_bg3HandleCycles = 0;
static bool s_bg3HandleDone = false; // false during the boot HANDLE handshake, true once past it

static std::deque<u8> s_v257RxFifo;                   // bytes Ridge Racer V reads back from its drive board
static u32 s_v257Accum = 0;                           // throttles the periodic refill
static constexpr u8 V257_STATUS[3] = {'C', '0', '1'}; // drive-board OK status; accepted by every RRV build

#ifdef _WIN32
static HANDLE s_imasPipe = INVALID_HANDLE_VALUE;
static bool s_imasPipeWarned = false;
static ULONGLONG s_imasPipeNextRetry = 0;
static std::deque<u8> s_imasTxFifo;
static std::deque<u8> s_imasRxFifo;
static std::deque<std::vector<u8>> s_imasWriteQueue;
static OVERLAPPED s_imasWriteOverlapped = {};
static bool s_imasWriteEventValid = false;
static bool s_imasWritePending = false;
static std::vector<u8> s_imasWriteBuffer;
static std::vector<u8> s_imasPacket;
static bool s_imasSentPacket = false;
static u32 s_imasIrqAccum = 0;
static u32 s_imasTxLogCount = 0;
static u32 s_imasRxLogCount = 0;
static u32 s_imasPacketLogCount = 0;

static void CloseImasPipe()
{
	if (s_imasPipe != INVALID_HANDLE_VALUE)
	{
		if (s_imasWritePending)
			CancelIo(s_imasPipe);
		CloseHandle(s_imasPipe);
		s_imasPipe = INVALID_HANDLE_VALUE;
	}
	if (s_imasWriteEventValid)
	{
		CloseHandle(s_imasWriteOverlapped.hEvent);
		s_imasWriteOverlapped = {};
		s_imasWriteEventValid = false;
	}
	s_imasWritePending = false;
	s_imasWriteBuffer.clear();
	s_imasWriteQueue.clear();
	s_imasTxFifo.clear();
	s_imasRxFifo.clear();
	s_imasPacket.clear();
	s_imasSentPacket = false;
	s_imasPipeNextRetry = GetTickCount64() + 1000;
}

static bool IsImas()
{
	return ACJV::GetGameId() == "NM00022";
}

static HANDLE GetImasPipe()
{
	if (s_imasPipe != INVALID_HANDLE_VALUE)
		return s_imasPipe;

	const ULONGLONG now = GetTickCount64();
	if (now < s_imasPipeNextRetry)
		return INVALID_HANDLE_VALUE;

	s_imasPipe = CreateFileW(LR"(\\.\pipe\imas)", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
	if (s_imasPipe == INVALID_HANDLE_VALUE && !s_imasPipeWarned)
	{
		Console.WriteLn("ACUART: imas pipe not available, continuing without external serial device.");
		s_imasPipeWarned = true;
	}
	if (s_imasPipe == INVALID_HANDLE_VALUE)
		s_imasPipeNextRetry = now + 1000;
	return s_imasPipe;
}

static bool HasImasPipeData()
{
	if (!s_imasSentPacket)
		return false;

	if (!s_imasRxFifo.empty())
		return true;

	HANDLE pipe = GetImasPipe();
	if (pipe == INVALID_HANDLE_VALUE)
		return false;

	DWORD available = 0;
	if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
	{
		CloseImasPipe();
		return false;
	}
	return available != 0;
}

static bool ReadImasPipeBlock(HANDLE pipe, u8* data, DWORD size, DWORD* read)
{
	*read = 0;

	OVERLAPPED overlapped = {};
	overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!overlapped.hEvent)
	{
		CloseImasPipe();
		return false;
	}

	BOOL result = ReadFile(pipe, data, size, read, &overlapped);
	if (!result)
	{
		const DWORD error = GetLastError();
		if (error == ERROR_IO_PENDING)
		{
			if (WaitForSingleObject(overlapped.hEvent, 0) != WAIT_OBJECT_0 ||
				!GetOverlappedResult(pipe, &overlapped, read, FALSE))
			{
				CancelIoEx(pipe, &overlapped);
				CloseHandle(overlapped.hEvent);
				return false;
			}
		}
		else if (error != ERROR_MORE_DATA)
		{
			CloseHandle(overlapped.hEvent);
			CloseImasPipe();
			return false;
		}
	}

	CloseHandle(overlapped.hEvent);
	return *read != 0;
}

static void DrainImasPipeInput()
{
	s_imasRxFifo.clear();

	HANDLE pipe = GetImasPipe();
	if (pipe == INVALID_HANDLE_VALUE)
		return;

	for (;;)
	{
		DWORD available = 0;
		if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
		{
			CloseImasPipe();
			return;
		}
		if (available == 0)
			return;

		u8 buffer[256];
		DWORD read = 0;
		if (!ReadImasPipeBlock(pipe, buffer, std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer))), &read))
			return;
	}
}

static void PumpImasPipeRead()
{
	if (!s_imasRxFifo.empty())
		return;

	HANDLE pipe = GetImasPipe();
	if (pipe == INVALID_HANDLE_VALUE)
		return;

	DWORD available = 0;
	if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
	{
		CloseImasPipe();
		return;
	}
	if (available == 0)
		return;

	std::vector<u8> buffer(std::min<DWORD>(available, 4096));

	DWORD read = 0;
	if (!ReadImasPipeBlock(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
		return;

	for (DWORD i = 0; i < read; i++)
	{
		if (s_imasRxLogCount < 64)
		{
			Console.WriteLn("ACUART: imas RX %02X", buffer[i]);
			s_imasRxLogCount++;
		}
		s_imasRxFifo.push_back(buffer[i]);
	}
}

static bool ReadImasPipe(u8* value)
{
	if (!s_imasSentPacket)
		return false;

	PumpImasPipeRead();
	if (s_imasRxFifo.empty())
		return false;

	*value = s_imasRxFifo.front();
	s_imasRxFifo.pop_front();
	return true;
}

static bool EnsureImasWriteEvent()
{
	if (s_imasWriteEventValid)
		return true;

	s_imasWriteOverlapped = {};
	s_imasWriteOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	s_imasWriteEventValid = s_imasWriteOverlapped.hEvent != nullptr;
	if (!s_imasWriteEventValid)
		CloseImasPipe();
	return s_imasWriteEventValid;
}

static void PumpImasPipeWrite()
{
	HANDLE pipe = GetImasPipe();
	if (pipe == INVALID_HANDLE_VALUE)
		return;

	if (s_imasWritePending)
	{
		DWORD written = 0;
		if (!GetOverlappedResult(pipe, &s_imasWriteOverlapped, &written, FALSE))
		{
			const DWORD error = GetLastError();
			if (error == ERROR_IO_INCOMPLETE)
				return;

			if (s_imasPacketLogCount < 16)
			{
				Console.WriteLn("ACUART: imas packet write failed (%u)", static_cast<unsigned>(error));
				s_imasPacketLogCount++;
			}
			CloseImasPipe();
			return;
		}

		if (s_imasPacketLogCount < 16)
		{
			Console.WriteLn("ACUART: imas packet write ok (%u bytes)", static_cast<unsigned>(written));
			s_imasPacketLogCount++;
		}
		s_imasWritePending = false;
		s_imasWriteBuffer.clear();
		ResetEvent(s_imasWriteOverlapped.hEvent);
	}

	if (s_imasWriteQueue.empty())
		return;
	if (!EnsureImasWriteEvent())
		return;

	s_imasWriteBuffer = std::move(s_imasWriteQueue.front());
	s_imasWriteQueue.pop_front();

	DWORD written = 0;
	if (WriteFile(pipe, s_imasWriteBuffer.data(), static_cast<DWORD>(s_imasWriteBuffer.size()), &written, &s_imasWriteOverlapped))
	{
		if (s_imasPacketLogCount < 16)
		{
			Console.WriteLn("ACUART: imas packet write ok (%u bytes)", static_cast<unsigned>(written));
			s_imasPacketLogCount++;
		}
		s_imasWriteBuffer.clear();
		ResetEvent(s_imasWriteOverlapped.hEvent);
		return;
	}

	const DWORD error = GetLastError();
	if (error == ERROR_IO_PENDING)
	{
		s_imasWritePending = true;
		return;
	}

	if (s_imasPacketLogCount < 16)
	{
		Console.WriteLn("ACUART: imas packet write failed (%u)", static_cast<unsigned>(error));
		s_imasPacketLogCount++;
	}
	CloseImasPipe();
}

static void FlushImasPipeWrites()
{
	if (!s_imasTxFifo.empty())
	{
		std::vector<u8> block;
		block.reserve(std::min<size_t>(s_imasTxFifo.size(), 256));
		while (!s_imasTxFifo.empty() && block.size() < 256)
		{
			block.push_back(s_imasTxFifo.front());
			s_imasTxFifo.pop_front();
		}
		if (s_imasWriteQueue.size() >= 16)
			s_imasWriteQueue.pop_front();
		s_imasWriteQueue.push_back(std::move(block));
	}
	PumpImasPipeWrite();
}

static void WriteImasPipe(u8 value)
{
	if (s_imasTxLogCount < 64)
	{
		Console.WriteLn("ACUART: imas TX %02X", value);
		s_imasTxLogCount++;
	}

	if (s_imasPacket.empty() && value != 0x02)
	{
		if (s_imasTxFifo.size() >= 4096)
			s_imasTxFifo.pop_front();
		s_imasTxFifo.push_back(value);
		FlushImasPipeWrites();
		return;
	}

	s_imasPacket.push_back(value);
	if (s_imasPacket.size() < 2)
		return;

	const size_t packet_size = static_cast<size_t>(s_imasPacket[1]) + 2;
	if (s_imasPacket.size() < packet_size)
		return;

	if (!s_imasSentPacket)
		DrainImasPipeInput();

	for (u8 b : s_imasPacket)
	{
		if (s_imasTxFifo.size() >= 4096)
			s_imasTxFifo.pop_front();
		s_imasTxFifo.push_back(b);
	}
	s_imasPacket.clear();
	FlushImasPipeWrites();
	s_imasSentPacket = true;
}
#endif

u16 ACUART::Read16(u32 addr) {
	const u32 reg = addr & 0xFFF;
	switch (reg) {
	case 0x000: // RBR or DLL
		if (uart_lcr & 0x80)
			return uart_dll;
		if (!s_v257RxFifo.empty()) // RRV reading the serial port: hand it the next status byte we queued ("E00")
		{
			const u8 b = s_v257RxFifo.front();
			s_v257RxFifo.pop_front();
			return b;
		}
		return 0; // no RX data
	case 0x002: // IER or DLH
		if (uart_lcr & 0x80)
			return uart_dlh;
		return uart_ier;
	case 0x004: // IIR (read-only)
		return 0x01; // no interrupt pending, 16550 mode
	case 0x006: // LCR
		return uart_lcr;
	case 0x008: // MCR
		return uart_mcr;
	case 0x00A: // LSR
		// bit 5 = THRE (TX holding register empty)
		// bit 6 = TEMT (TX shift register empty)
		// both set = transmitter idle, ready to accept data
		// bit 0 = DR (RX data ready) — set while the V257 status FIFO has bytes (RRV)
#ifdef _WIN32
		if (IsImas())
		{
			return 0x60 | (HasImasPipeData() ? 0x01 : 0x00);
		}
#endif
		return 0x60 | (s_v257RxFifo.empty() ? 0x00 : 0x01);
	case 0x00C: // MSR
		return 0;
	case 0x00E: // SCR
		return uart_scr;
	default:
		return 0;
	}
}

// Battle Gear 3 / Tuned: answer the steering board's boot handshake (BGRLOAD FUN_00133e60) so the game
// doesn't stall on "HANDLE ERROR". With no emulated FFB board we reply ready and keep the drive-board flag
// (EE 0x2694b0) CLEAR, so steering stays on the JVS analog wheel. FFB GROUNDWORK: set s_bg3FfbEnabled=true
// when a real drive board is emulated to run the full calibration handshake.
// (Thanks to Hydreigon223 for the dump.)
static void Bg3HandleReply(u8 curTx)
{
	s_bg3TxCnt++;
	if ((s_bg3TxCnt & 1) != 0) { s_bg3PrevTx = curTx; return; } // a {reg,val} command is 2 bytes; reply on the 2nd

	if (s_bg3PrevTx == 0x20 && curTx == 0x00) // {0x20,0} starts an init -> re-arm so a re-init replies fresh
	{
		s_bg3HandleDone = false;
		s_bg3HandleCycles = 0;
	}
	u8 hi = 0x00;
	if (!s_bg3HandleDone)
	{
		static constexpr bool s_bg3FfbEnabled = false;    // true once a real FFB drive board is emulated
		if (s_bg3PrevTx == 0x20 || s_bg3PrevTx == 0x1f)
			hi = s_bg3FfbEnabled ? 0x80 : 0x01;           // 0x01 = ready (skip calibrate while FFB off)
		else if (s_bg3PrevTx == 0x14 && curTx == 0x1a)
			hi = (++s_bg3HandleCycles > 4) ? 0x00 : 0x80; // calibrate busy-loop (FFB on only)
		if (s_bg3PrevTx == 0x11 && curTx == 0x03)
			s_bg3HandleDone = true;                       // last init command
	}
	s_v257RxFifo.push_back(hi); // byte0 = busy/ready status
	s_v257RxFifo.push_back(0);  // byte1
	ACCORE::intr(ACCORE::INTRN_UART);
	s_bg3PrevTx = curTx;
}

void ACUART::Write16(u32 addr, u16 val) {
	const u32 reg = addr & 0xFFF;
	switch (reg) {
	case 0x000: // THR or DLL
		if (uart_lcr & 0x80)
			uart_dll = val;
		else // TX byte to the drive board
		{
			const std::string& gdrv = ACJV::GetGameId();
#ifdef _WIN32
			if (gdrv == "NM00022")
				WriteImasPipe((u8)val);
#endif
			if (gdrv == "NM00010" || gdrv == "NM00015")
				Bg3HandleReply((u8)val); // BG3/Tuned: answer the HANDLE handshake (other games ignore TX)
		}
		break;
	case 0x002: // IER or DLH — set to 0 on module stop, bits toggled during xmit (acUartModuleStop, uart_xmit)
		if (uart_lcr & 0x80)
			uart_dlh = val;
		else
			uart_ier = val;
		break;
	case 0x004: // FCR (write-only) — val=7 on module stop: enable FIFO + reset RX/TX (acUartModuleStop)
		uart_fcr_shadow = val & 0xC9; // preserve trigger level + enable bits
		break;
	case 0x006: // LCR
		uart_lcr = val;
		break;
	case 0x008: // MCR
		uart_mcr = val;
		break;
	case 0x00E: // SCR
		uart_scr = val;
		break;
	default:
		break;
	}
}

void ACUART::ResetBg3State()
{
	// Re-arm the BG3 HANDLE handshake on each game boot/reset, else a reset stalls on "HANDLE ERROR".
	s_bg3TxCnt = 0;
	s_bg3PrevTx = 0;
	s_bg3HandleCycles = 0;
	s_bg3HandleDone = false;
#ifdef _WIN32
	CloseImasPipe();
	s_imasPipeWarned = false;
	s_imasPipeNextRetry = 0;
	s_imasIrqAccum = 0;
	s_imasTxLogCount = 0;
	s_imasRxLogCount = 0;
	s_imasPacketLogCount = 0;
#endif
}

void ACUART::StreamImas(u32 cycles)
{
#ifdef _WIN32
	if (!IsImas())
		return;

	if (s_imasWritePending || !s_imasWriteQueue.empty() || !s_imasTxFifo.empty())
		PumpImasPipeWrite();
	if (!(uart_ier & 0x01))
		return;

	s_imasIrqAccum += cycles;
	if (s_imasIrqAccum < 240)
		return;
	s_imasIrqAccum = 0;

	if (HasImasPipeData())
		ACCORE::intr(ACCORE::INTRN_UART);
#endif
}

// Ridge Racer V drive-board status streamer (called each DEV9 tick): refill the receive buffer with the
// board's OK status and raise the RX interrupt. Only RRV needs this.
void ACUART::StreamV257(u32 cycles)
{
	const std::string& gid2 = ACJV::GetGameId();
	if (gid2 == "NM00010" || gid2 == "NM00015")
		return; // BG3: the HANDLE responder is driven synchronously from Write16; leave the RX FIFO intact
	if (gid2 != "NM00001") // only Ridge Racer V needs the streaming responder
	{
		s_v257RxFifo.clear();
		return;
	}
	if (!(uart_ier & 0x01))        // host hasn't enabled the RX-data interrupt yet
		return;
	s_v257Accum += cycles;
	if (s_v257Accum < 240)         // throttle (DEV9async ticks ~tens of kHz with cycles=1) -> a few hundred Hz
		return;
	s_v257Accum = 0;
	// Keep raising the RX IRQ every tick; reload the status only once the ISR has drained the previous copy.
	if (s_v257RxFifo.empty())
		s_v257RxFifo.assign(V257_STATUS, V257_STATUS + 3);
	ACCORE::intr(ACCORE::INTRN_UART); // raise the UART RX interrupt
}
