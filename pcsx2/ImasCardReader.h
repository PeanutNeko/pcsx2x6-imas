// SPDX-FileCopyrightText: 2026 PCSX2x6-imas contributors
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

namespace ImasCardReader
{
	u32 Read(u8* data, u32 size);
	u32 Write(const u8* data, u32 size);
	void Pump();
}
