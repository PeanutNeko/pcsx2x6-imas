// SPDX-FileCopyrightText: 2026 PCSX2x6-imas contributors
// SPDX-License-Identifier: GPL-3.0+

#include "DEV9/ImasJVS.h"

#include "DEV9/ACJV.h"

#include <chrono>

namespace ImasJVS
{
	static bool s_active = false;
	static bool s_left = true;
	static bool s_right = false;
	static bool s_button_2 = false;
	static u64 s_begin_time = 0;

	static u64 GetMilliseconds()
	{
		return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	void SetActive(bool active)
	{
		s_active = active;
		s_left = true;
		s_right = false;
		s_button_2 = false;
		s_begin_time = GetMilliseconds();
	}

	void AppendFeatures(u8*& output, u8* packet_size)
	{
		if (!s_active)
			return;

		(*output++) = 0x12; // GPIO output
		(*output++) = 0x06; // slot count
		(*output++) = 0x00;
		(*output++) = 0x00;
		(*packet_size) += 4;
	}

	u16 ApplyButtonState(u16 buttons, u16 macro_buttons)
	{
		if (!s_active)
			return buttons;

		buttons = s_button_2 ? (buttons | JVS_BTN_2) : (buttons & static_cast<u16>(~JVS_BTN_2));
		buttons = s_left ? (buttons | JVS_BTN_LEFT) : (buttons & static_cast<u16>(~JVS_BTN_LEFT));
		buttons = s_right ? (buttons | JVS_BTN_RIGHT) : (buttons & static_cast<u16>(~JVS_BTN_RIGHT));
		return buttons | macro_buttons;
	}

	void HandleGeneralOutput(u8 value)
	{
		if (!s_active)
			return;

		const u64 now = GetMilliseconds();
		if (value & 0x10)
		{
			const u64 phase = (now - s_begin_time) % 6000;
			s_left = phase < 1000;
			s_button_2 = phase > 2000 && phase < 5000;
			s_right = phase > 3000 && phase < 4000;
		}
		else
		{
			s_begin_time = now;
			s_left = true;
			s_right = false;
			s_button_2 = false;
		}
	}
}
