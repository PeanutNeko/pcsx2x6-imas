#include "ACCORE.h"
#include "ACATAPI.h"
#include "ACATA.h"
#include "common/Console.h"
#include "common/FileSystem.h"

#include "ACMACROS.h"

#include <sys/stat.h>
#include <cstring>

static u8 atapi_pio_buf[65536];
static u32 atapi_pio_len = 0;
static u32 atapi_pio_pos = 0;

static u8 atapi_pio_write_buf[65536];
static u32 atapi_pio_write_len = 0;
static u32 atapi_pio_write_pos = 0;
static u8 mode_page_01[12] = {};
static bool mode_page_01_set = false;

static void atapi_pio_write_setup(u32 len) {
    atapi_pio_write_len = len;
    atapi_pio_write_pos = 0;
    memset(atapi_pio_write_buf, 0, std::min(len, (u32)sizeof(atapi_pio_write_buf)));
    ACATA::R_LCYL = len & 0xFF;
    ACATA::R_HCYL = (len >> 8) & 0xFF;
    ACATA::R_NSECTOR = 0x00;
    ACATA::R_STATUS |= ATA_STAT_DRQ;
    CLRB(ACATA::R_STATUS, ATA_STAT_BUSY);
    CLRB(ACATA::R_STATUS, ATA_STAT_ERR);
    ACCORE::intr(ACCORE::INTRN_ATA);
}

static void atapi_pio_setup(u32 len) {
    atapi_pio_len = len;
    atapi_pio_pos = 0;
    ACATA::R_LCYL = len & 0xFF;
    ACATA::R_HCYL = (len >> 8) & 0xFF;
    ACATA::R_NSECTOR = 0x02;
    ACATA::R_STATUS |= ATA_STAT_DRQ;
    CLRB(ACATA::R_STATUS, ATA_STAT_BUSY);
    CLRB(ACATA::R_STATUS, ATA_STAT_ERR);
    ACCORE::intr(ACCORE::INTRN_ATA);
}

static void atapi_complete_nodata() {
    atapi_pio_len = 0;
    atapi_pio_pos = 0;
    ACATA::R_NSECTOR = 0x03;
    CLRB(ACATA::R_STATUS, ATA_STAT_DRQ);
    CLRB(ACATA::R_STATUS, ATA_STAT_ERR);
    CLRB(ACATA::R_STATUS, ATA_STAT_BUSY);
    ACATA::R_STATUS |= ATA_STAT_READY;
    ACATA::R_LCYL = 0;
    ACATA::R_HCYL = 0;
    ACCORE::intr(ACCORE::INTRN_ATA);
}

u16 ACATAPI::pio_read_word() {
    if (atapi_pio_pos * 2 < atapi_pio_len) {
        u32 offset = atapi_pio_pos * 2;
        u16 val = atapi_pio_buf[offset];
        if (offset + 1 < atapi_pio_len)
            val |= (u16)atapi_pio_buf[offset + 1] << 8;
        atapi_pio_pos++;
        if (atapi_pio_pos * 2 >= atapi_pio_len) {
            ACATA::R_NSECTOR = 0x03;
            CLRB(ACATA::R_STATUS, ATA_STAT_DRQ);
            ACATA::R_STATUS |= ATA_STAT_READY;
            ACCORE::intr(ACCORE::INTRN_ATA);
        }
        return val;
    }
    return 0;
}

bool ACATAPI::has_pio_data() {
    return atapi_pio_len > 0 && atapi_pio_pos * 2 < atapi_pio_len;
}

void ACATAPI::pio_write_word(u16 val) {
    if (atapi_pio_write_pos * 2 >= atapi_pio_write_len)
        return;
    u32 offset = atapi_pio_write_pos * 2;
    atapi_pio_write_buf[offset] = val & 0xFF;
    if (offset + 1 < atapi_pio_write_len)
        atapi_pio_write_buf[offset + 1] = (val >> 8) & 0xFF;
    atapi_pio_write_pos++;
    if (atapi_pio_write_pos * 2 >= atapi_pio_write_len) {
        if (atapi_pio_write_len >= 12 && atapi_pio_write_buf[8] == 0x01) {
            memcpy(mode_page_01, atapi_pio_write_buf, 12);
            mode_page_01_set = true;
        }
        atapi_pio_write_len = 0;
        atapi_complete_nodata();
    }
}

bool ACATAPI::has_pio_write() {
    return atapi_pio_write_len > 0 && atapi_pio_write_pos * 2 < atapi_pio_write_len;
}

void ACATAPI::handle_cmd(atapi_packet_t P) {
    u32 transf_lba = ATAPI_PKT_GETLBA(P);
    u32 nsec = ATAPI_PKT_GETLEN(P);
    switch (P.pkt.opcode) {
    case ATAPICMD::TEST_UNIT_READY:
        atapi_complete_nodata();
        break;

    case ATAPICMD::READ_CAPACITY: {
        if ((ACATA::TH::IMAGE || ACATA::TH::isCHD) && ACATA::TH::IMAGESIZE > 0) {
            u32 last_lba = (u32)(ACATA::TH::IMAGESIZE / ACATAPI::CONSTANTS::DVD_SECTORSIZE) - 1;
            u32 block_len = ACATAPI::CONSTANTS::DVD_SECTORSIZE;
            atapi_pio_buf[0] = (last_lba >> 24) & 0xFF;
            atapi_pio_buf[1] = (last_lba >> 16) & 0xFF;
            atapi_pio_buf[2] = (last_lba >>  8) & 0xFF;
            atapi_pio_buf[3] = last_lba & 0xFF;
            atapi_pio_buf[4] = (block_len >> 24) & 0xFF;
            atapi_pio_buf[5] = (block_len >> 16) & 0xFF;
            atapi_pio_buf[6] = (block_len >>  8) & 0xFF;
            atapi_pio_buf[7] = block_len & 0xFF;
            atapi_pio_setup(8);
        } else {
            Console.Error("ACATAPI:READ_CAPACITY: image not open or size invalid");
            ACATA::R_STATUS |= ATA_STAT_ERR;
            ACATA::R_ERROR = ATA_ERR_ABORT;
        }
        break;
    }

    case ATAPICMD::MODE_SENSE: {
        u8 page_code = P.raw8[2] & 0x3F;
        u16 alloc_len = (P.raw8[7] << 8) | P.raw8[8];
        memset(atapi_pio_buf, 0, 64);
        u32 resp_len = 0;
        if (page_code == 0x01) {
            resp_len = 20;
            if (mode_page_01_set) {
                memcpy(atapi_pio_buf, mode_page_01, 12);
            } else {
                atapi_pio_buf[0] = 0x00; atapi_pio_buf[1] = 0x12;
                atapi_pio_buf[2] = (ACATA::MediaType == ACMEDIATYPE::ACDVD) ? 0x41 :
                                   (ACATA::MediaType == ACMEDIATYPE::ACCD)  ? 0x01 : 0x00;
                atapi_pio_buf[8] = 0x01; atapi_pio_buf[9] = 0x0A;
                atapi_pio_buf[11] = 0x05;
            }
        } else if (page_code == 0x2A) {
            resp_len = 28;
            atapi_pio_buf[0] = 0x00; atapi_pio_buf[1] = 0x1A;
            atapi_pio_buf[8] = 0x2A; atapi_pio_buf[9] = 0x12;
            atapi_pio_buf[10] = 0x03;
            atapi_pio_buf[16] = 0x15; atapi_pio_buf[17] = 0xA4;
            atapi_pio_buf[22] = 0x15; atapi_pio_buf[23] = 0xA4;
        } else {
            resp_len = 8;
            atapi_pio_buf[0] = 0x00; atapi_pio_buf[1] = 0x06;
        }
        if (alloc_len > 0 && alloc_len < resp_len)
            resp_len = alloc_len;
        atapi_pio_setup(resp_len);
        break;
    }

    case ATAPICMD::READ_10: {
        //Console.Warning("ACATAPI:READ_10: tlen:%X, lba:%X", P.pkt.transf_len, transf_lba);
        if ((!ACATA::TH::IMAGE && !ACATA::TH::isCHD) || nsec == 0) {
            Console.Error("ACATAPI:READ_10: no image or zero sectors");
            ACATA::R_STATUS |= ATA_STAT_ERR;
            ACATA::R_ERROR = ATA_ERR_ABORT;
            atapi_complete_nodata();
            break;
        }
        if (ACATA_ISDMA) {
            ACATA::TH::LBA = transf_lba;
            ACATA::TH::nsector = nsec;
            ACATA::TH::sectorsize = ACATAPI::CONSTANTS::DVD_SECTORSIZE;
            ACCORE::DMA::PendTrasnfType = ACCORE::DMA::ATAPI;
            ACATA::R_NSECTOR = 0x02;
            CLRB(ACATA::R_STATUS, ATA_STAT_DRQ);
            CLRB(ACATA::R_STATUS, ATA_STAT_ERR);
            ACCORE::intr(ACCORE::INTRN_ATA);
        } else {
            u32 total = nsec * ACATAPI::CONSTANTS::DVD_SECTORSIZE;
            if (total > sizeof(atapi_pio_buf)) {
                Console.Error("ACATAPI:READ_10: transfer too large (%u bytes)", total);
                ACATA::R_STATUS |= ATA_STAT_ERR;
                ACATA::R_ERROR = ATA_ERR_ABORT;
                atapi_complete_nodata();
                break;
            }
            s64 offset = (s64)transf_lba * ACATAPI::CONSTANTS::DVD_SECTORSIZE;
            FileSystem::FSeek64(ACATA::TH::IMAGE, offset, SEEK_SET);
            size_t rd = fread(atapi_pio_buf, 1, total, ACATA::TH::IMAGE);
            if (rd == total) {
                atapi_pio_setup(total);
            } else {
                Console.Error("ACATAPI:READ_10: short read (%zu/%u)", rd, total);
                ACATA::R_STATUS |= ATA_STAT_ERR;
                ACATA::R_ERROR = ATA_ERR_ABORT;
                atapi_complete_nodata();
            }
        }
        break;
    }

    case ATAPICMD::MODE_SELECT: {
        u16 param_len = ATAPI_PKT_GETLEN(P);
        if (param_len > 0) {
            atapi_pio_write_setup(param_len);
        } else {
            atapi_complete_nodata();
        }
        break;
    }

    default:
        Console.Error("ACATAPI: UNK_CMD %02X, lba:%08X, nsec:%04X", P.raw8[0], transf_lba, nsec);
        break;
    }
}

void ACATAPI::Setup() {
    u32 SectorSizes[3] = {/*TODO: CHECK CD SECTOR SIZE*/0, ACATAPI::CONSTANTS::DVD_SECTORSIZE, 512};
    ACATA::TH::sectorsize = SectorSizes[ACATA::MediaType];
}
