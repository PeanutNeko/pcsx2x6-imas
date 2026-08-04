// SPDX-FileCopyrightText: 2026 PCSX2x6-imas contributors
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

namespace ImasJVS
{
	void SetActive(bool active);
	void AppendFeatures(u8*& output, u8* packet_size);
	u16 ApplyButtonState(u16 buttons, u16 macro_buttons);
	void HandleGeneralOutput(u8 value);
}
