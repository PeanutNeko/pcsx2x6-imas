#include "ACATA_IO_CHD.h"
#include "common/StringUtil.h"
#include "common/Console.h"
#include <cstring>

ChdImage::ChdImage() = default;

ChdImage::~ChdImage()
{
    Close();
}

bool ChdImage::Open(const std::string& path)
{
    Close();

    chd_error err = chd_open(path.c_str(), CHD_OPEN_READ, nullptr, &m_chd);

    if (err != CHDERR_NONE) {
        Console.ErrorFmt("{} failed to open CHD: {}", __FUNCTION__, (int)err);
        return false;
    }

    const chd_header* hdr = chd_get_header(m_chd);

    m_hunkSize = hdr->hunkbytes;

    switch (hdr->unitbytes)
    {
        case 2048:
            m_type = ACMEDIATYPE::ACDVD;
            break;

        case 512:
            m_type = ACMEDIATYPE::ACHDD;
            break;

        default:
            m_type = ACMEDIATYPE::ACUNK;
            break;
    }

    m_unitBytes = hdr->unitbytes;
    m_totalUnits = hdr->logicalbytes / hdr->unitbytes;

    m_hunkBuffer.resize(m_hunkSize);

    return true;
}

void ChdImage::Close()
{
    if (m_chd)
    {
        chd_close(m_chd);
        m_chd = nullptr;
    }

    m_hunkBuffer.clear();

    m_cachedHunk = UINT32_MAX;

    m_hunkSize = 0;
    m_unitBytes = 0;
    m_totalUnits = 0;

    m_type = ACMEDIATYPE::ACUNK;
}

bool ChdImage::IsOpen() const
{
    return m_chd != nullptr;
}

ACMEDIATYPE ChdImage::GetType() const
{
    return m_type;
}

u32 ChdImage::GetSectorSize() const
{
    return m_unitBytes;
}

u64 ChdImage::GetSectorCount() const
{
    return m_totalUnits;
}

bool ChdImage::ReadHunk(u32 hunk)
{
    if (hunk == m_cachedHunk)
        return true;

    chd_error err =
        chd_read(m_chd,
                 hunk,
                 m_hunkBuffer.data());

    if (err != CHDERR_NONE)
        return false;

    m_cachedHunk = hunk;
    return true;
}

bool ChdImage::ReadSector(u64 lba, void* buffer)
{
    if (!m_chd)
        return false;

    if (lba >= m_totalUnits)
        return false;

    const u64 byteOffset = lba * m_unitBytes;

    const u32 hunk =
        static_cast<u32>(byteOffset / m_hunkSize);

    const u32 offset =
        static_cast<u32>(byteOffset % m_hunkSize);

    if ((offset + m_unitBytes) > m_hunkSize)
        return false;

    if (!ReadHunk(hunk))
        return false;

    std::memcpy(
        buffer,
        m_hunkBuffer.data() + offset,
        m_unitBytes);

    return true;
}

bool ChdImage::ReadSectors(u64 lba,
                           u32 count,
                           void* buffer)
{
    u8* dst = static_cast<u8*>(buffer);

    for (u32 i = 0; i < count; i++)
    {
        if (!ReadSector(lba + i,
                        dst + (i * m_unitBytes)))
        {
            return false;
        }
    }

    return true;
}

bool ChdImage::IsChdFileName(const std::string& path)
{
	return StringUtil::compareNoCase(Path::GetExtension(path), "chd");
}
