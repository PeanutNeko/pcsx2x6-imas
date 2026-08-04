// SPDX-FileCopyrightText: 2026 PCSX2x6-imas contributors
// SPDX-License-Identifier: GPL-3.0+

#include "ImasCardReader.h"

#include "common/Console.h"

#include <algorithm>
#include <deque>
#include <vector>

#ifdef _WIN32
#include "common/RedtapeWindows.h"

namespace ImasCardReader
{
	static HANDLE s_pipe = INVALID_HANDLE_VALUE;
	static bool s_pipe_warned = false;
	static OVERLAPPED s_write_overlapped = {};
	static bool s_write_event_valid = false;
	static bool s_write_pending = false;
	static std::vector<u8> s_write_buffer;
	static std::deque<std::vector<u8>> s_write_queue;
	static bool s_sent_packet = false;
	static u32 s_tx_log_count = 0;
	static u32 s_rx_log_count = 0;
	static u32 s_packet_log_count = 0;

	static void ClosePipe()
	{
		if (s_pipe != INVALID_HANDLE_VALUE)
		{
			if (s_write_pending)
				CancelIo(s_pipe);
			CloseHandle(s_pipe);
			s_pipe = INVALID_HANDLE_VALUE;
		}
		if (s_write_event_valid)
		{
			CloseHandle(s_write_overlapped.hEvent);
			s_write_overlapped = {};
			s_write_event_valid = false;
		}
		s_write_pending = false;
		s_write_buffer.clear();
		s_write_queue.clear();
		s_sent_packet = false;
	}

	static HANDLE GetPipe()
	{
		if (s_pipe != INVALID_HANDLE_VALUE)
			return s_pipe;

		s_pipe = CreateFileW(LR"(\\.\pipe\imas)", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (s_pipe == INVALID_HANDLE_VALUE && !s_pipe_warned)
		{
			Console.WriteLn("ACUART HLE: imas pipe not available.");
			s_pipe_warned = true;
		}
		return s_pipe;
	}

	static bool EnsureWriteEvent()
	{
		if (s_write_event_valid)
			return true;

		s_write_overlapped = {};
		s_write_overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		s_write_event_valid = s_write_overlapped.hEvent != nullptr;
		if (!s_write_event_valid)
			ClosePipe();
		return s_write_event_valid;
	}

	void Pump()
	{
		if (!s_write_pending && s_write_queue.empty())
			return;

		HANDLE pipe = GetPipe();
		if (pipe == INVALID_HANDLE_VALUE)
			return;

		if (s_write_pending)
		{
			DWORD written = 0;
			if (!GetOverlappedResult(pipe, &s_write_overlapped, &written, FALSE))
			{
				const DWORD error = GetLastError();
				if (error == ERROR_IO_INCOMPLETE)
					return;

				if (s_packet_log_count < 16)
				{
					Console.WriteLn("ACUART HLE: imas packet write failed (%u)", static_cast<unsigned>(error));
					s_packet_log_count++;
				}
				ClosePipe();
				return;
			}

			if (written != s_write_buffer.size())
			{
				if (s_packet_log_count < 16)
				{
					Console.WriteLn("ACUART HLE: imas packet short write (%u/%u bytes)",
						static_cast<unsigned>(written), static_cast<unsigned>(s_write_buffer.size()));
					s_packet_log_count++;
				}
				ClosePipe();
				return;
			}

			if (s_packet_log_count < 16)
			{
				Console.WriteLn("ACUART HLE: imas packet write ok (%u bytes)", static_cast<unsigned>(written));
				s_packet_log_count++;
			}
			s_write_pending = false;
			s_write_buffer.clear();
			ResetEvent(s_write_overlapped.hEvent);
		}

		if (s_write_queue.empty() || !EnsureWriteEvent())
			return;

		s_write_buffer = std::move(s_write_queue.front());
		s_write_queue.pop_front();
		if (s_write_buffer.empty())
			return;

		DWORD written = 0;
		if (WriteFile(pipe, s_write_buffer.data(), static_cast<DWORD>(s_write_buffer.size()),
			&written, &s_write_overlapped))
		{
			if (written != s_write_buffer.size())
			{
				if (s_packet_log_count < 16)
				{
					Console.WriteLn("ACUART HLE: imas packet short write (%u/%u bytes)",
						static_cast<unsigned>(written), static_cast<unsigned>(s_write_buffer.size()));
					s_packet_log_count++;
				}
				ClosePipe();
				return;
			}

			if (s_packet_log_count < 16)
			{
				Console.WriteLn("ACUART HLE: imas packet write ok (%u bytes)", static_cast<unsigned>(written));
				s_packet_log_count++;
			}
			s_write_buffer.clear();
			ResetEvent(s_write_overlapped.hEvent);
			return;
		}

		const DWORD error = GetLastError();
		if (error == ERROR_IO_PENDING)
		{
			s_write_pending = true;
			return;
		}

		if (s_packet_log_count < 16)
		{
			Console.WriteLn("ACUART HLE: imas packet write failed (%u)", static_cast<unsigned>(error));
			s_packet_log_count++;
		}
		ClosePipe();
	}

	static bool ReadBlock(HANDLE pipe, u8* data, DWORD size, DWORD* read)
	{
		*read = 0;
		OVERLAPPED overlapped = {};
		overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!overlapped.hEvent)
		{
			ClosePipe();
			return false;
		}

		if (!ReadFile(pipe, data, size, read, &overlapped))
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
				ClosePipe();
				return false;
			}
		}

		CloseHandle(overlapped.hEvent);
		return *read != 0;
	}

	static void DrainPipeInput(HANDLE pipe)
	{
		for (;;)
		{
			DWORD available = 0;
			if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
			{
				ClosePipe();
				return;
			}
			if (available == 0)
				return;

			u8 buffer[256];
			DWORD read = 0;
			if (!ReadBlock(pipe, buffer, std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer))), &read))
				return;
		}
	}

	u32 Read(u8* data, u32 size)
	{
		Pump();
		if (!s_sent_packet || size == 0)
			return 0;

		HANDLE pipe = GetPipe();
		if (pipe == INVALID_HANDLE_VALUE)
			return 0;

		DWORD available = 0;
		if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
		{
			ClosePipe();
			return 0;
		}
		if (available == 0)
			return 0;

		DWORD read = 0;
		if (!ReadBlock(pipe, data, std::min<DWORD>(size, available), &read))
			return 0;

		for (DWORD i = 0; i < read && s_rx_log_count < 64; i++)
		{
			Console.WriteLn("ACUART HLE: imas RX %02X", data[i]);
			s_rx_log_count++;
		}
		return read;
	}

	u32 Write(const u8* data, u32 size)
	{
		if (size == 0)
		{
			if (s_tx_log_count < 64)
			{
				Console.WriteLn("ACUART HLE: imas TX empty");
				s_tx_log_count++;
			}
			return 0;
		}

		Pump();
		HANDLE pipe = GetPipe();
		if (pipe == INVALID_HANDLE_VALUE)
			return 0;

		if (s_tx_log_count < 64)
			Console.WriteLn("ACUART HLE: imas TX packet size %u", static_cast<unsigned>(size));
		std::vector<u8> buffer(data, data + size);
		for (u8 value : buffer)
		{
			if (s_tx_log_count >= 64)
				break;
			Console.WriteLn("ACUART HLE: imas TX %02X", value);
			s_tx_log_count++;
		}

		if (!s_sent_packet)
			DrainPipeInput(pipe);
		if (s_write_queue.size() >= 16)
			s_write_queue.pop_front();
		s_write_queue.push_back(std::move(buffer));
		s_sent_packet = true;
		Pump();
		return size;
	}
}

#else

namespace ImasCardReader
{
	u32 Read(u8*, u32) { return 0; }
	u32 Write(const u8*, u32) { return 0; }
	void Pump() {}
}

#endif
