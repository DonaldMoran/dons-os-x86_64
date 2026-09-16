#include "include/ata.h"
#include "include/io.h"
#include "include/serial.h"
#include "include/vga.h"
#include "include/libc.h"

/* ------------------------------------------------------------------ *
 * Primary channel I/O ports (compatibility mode)
 * ------------------------------------------------------------------ */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LO      0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HI      0x1F5
#define ATA_PRIMARY_DRIVE_HEAD  0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_ALT_STATUS  0x3F6

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_FLUSH_CACHE     0xE7

/*
 * Drive/head register values.
 *
 * Bit 6 of this register is the "L" (LBA) bit:
 *   0 = CHS addressing mode
 *   1 = LBA addressing mode
 *
 * IDENTIFY DEVICE ignores the L bit, so the CHS-mode values (0xA0/0xB0)
 * work there. READ SECTORS and WRITE SECTORS require L=1 so the drive
 * interprets the SECCOUNT/LBA_LO/LBA_MID/LBA_HI registers as an LBA
 * rather than a CHS tuple. The original driver used 0xA0/0xB0 for
 * everything, which silently disabled LBA mode and made every read
 * command fail with ERR set and DRQ never asserted.
 */
#define ATA_DRIVE_MASTER_SEL    0xA0
#define ATA_DRIVE_SLAVE_SEL     0xB0
#define ATA_DRIVE_MASTER_LBA    0xE0    /* 0xA0 | 0x40 */
#define ATA_DRIVE_SLAVE_LBA     0xF0    /* 0xB0 | 0x40 */

/*
 * Refuse to write below this LBA on the master drive. The master
 * carries the boot chain, the kernel, and (in single-drive mode) the
 * FAT partition. The floor protects the entire pre-partition region:
 *
 *   LBA 0       : boot.bin     (stage1)
 *   LBA 1..127  : stage2.bin   (128 sectors reserved; ~68 used)
 *   LBA 128..   : kernel.bin   (up to ~960 KB before LBA 2048)
 *   LBA 2048+   : FAT16 partition (single-drive mode)
 *
 * 2048 is chosen so the floor sits exactly at the FAT partition start.
 * A stray write below it (from a corrupted BPB, say) is refused rather
 * than allowed to silently overwrite the kernel.
 *
 * This floor applies only to the master. The slave drive (dual-drive
 * mode's fat.img) is unrestricted.
 */
#define ATA_WRITE_PROTECT_LBAS  2048

static int    s_present[2] = {0, 0};
static char   s_model[2][41];
static uint8_t s_default_drive = ATA_DRIVE_MASTER;

/* ------------------------------------------------------------------ *
 * Low-level helpers
 * ------------------------------------------------------------------ */

static inline void ata_400ns_delay(void) {
    (void)inb(ATA_PRIMARY_ALT_STATUS);
    (void)inb(ATA_PRIMARY_ALT_STATUS);
    (void)inb(ATA_PRIMARY_ALT_STATUS);
    (void)inb(ATA_PRIMARY_ALT_STATUS);
}

static int ata_poll_bsy_clear(void) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_PRIMARY_STATUS);
        if (st == 0xFF) return -1;
        if ((st & ATA_SR_BSY) == 0) {
            if (st & ATA_SR_ERR) return -2;
            if (st & ATA_SR_DF)  return -2;
            return 0;
        }
    }
    return -1;
}

static int ata_poll_drq(void) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_PRIMARY_STATUS);
        if (st == 0xFF) return -1;
        if ((st & ATA_SR_BSY) == 0) {
            if (st & ATA_SR_ERR) return -2;
            if (st & ATA_SR_DF)  return -2;
            if (st & ATA_SR_DRQ) return 0;
        }
    }
    return -1;
}

/*
 * Select drive and program the head register with the top 4 LBA bits.
 * Uses the LBA-mode variants (bit 6 set) so that subsequent READ/WRITE
 * SECTORS commands interpret the LBA registers correctly.
 */
static void ata_select_drive(uint8_t drive, uint32_t lba) {
    uint8_t sel = (drive == ATA_DRIVE_MASTER)
                    ? ATA_DRIVE_MASTER_LBA
                    : ATA_DRIVE_SLAVE_LBA;
    sel |= (uint8_t)((lba >> 24) & 0x0F);
    outb(ATA_PRIMARY_DRIVE_HEAD, sel);
    ata_400ns_delay();
}

/* ------------------------------------------------------------------ *
 * IDENTIFY  (with step-by-step instrumentation)
 * ------------------------------------------------------------------ */

static int ata_identify_raw(uint8_t drive, uint16_t* out) {
    serial_print("ATA:   id_raw: enter drive=");
    serial_print_dec(drive);
    serial_print("\n");

    /* IDENTIFY ignores the L bit, so use the plain select values here. */
    uint8_t sel = (drive == ATA_DRIVE_MASTER)
                    ? ATA_DRIVE_MASTER_SEL
                    : ATA_DRIVE_SLAVE_SEL;

    serial_print("ATA:   id_raw: outb(0x1F6, 0x");
    serial_print_hex(sel);
    serial_print(")\n");
    outb(ATA_PRIMARY_DRIVE_HEAD, sel);
    serial_print("ATA:   id_raw: outb(0x1F6) done\n");

    ata_400ns_delay();
    serial_print("ATA:   id_raw: 400ns delay done\n");

    serial_print("ATA:   id_raw: zeroing sec count / LBA regs\n");
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO,   0);
    outb(ATA_PRIMARY_LBA_MID,  0);
    outb(ATA_PRIMARY_LBA_HI,   0);
    serial_print("ATA:   id_raw: LBA regs zeroed\n");

    serial_print("ATA:   id_raw: outb(0x1F7, 0xEC)\n");
    outb(ATA_PRIMARY_STATUS, ATA_CMD_IDENTIFY);
    serial_print("ATA:   id_raw: IDENTIFY command sent\n");

    ata_400ns_delay();

    uint8_t st = inb(ATA_PRIMARY_STATUS);
    serial_print("ATA:   id_raw: status after cmd = 0x");
    serial_print_hex(st);
    serial_print("\n");

    if (st == 0x00) {
        serial_print("ATA:   id_raw: no device (status 0x00)\n");
        return -1;
    }

    uint8_t mid = inb(ATA_PRIMARY_LBA_MID);
    uint8_t hi  = inb(ATA_PRIMARY_LBA_HI);
    serial_print("ATA:   id_raw: LBA mid=0x");
    serial_print_hex(mid);
    serial_print(" hi=0x");
    serial_print_hex(hi);
    serial_print("\n");

    if (mid != 0 || hi != 0) {
        serial_print("ATA:   id_raw: ATAPI device detected\n");
        return -2;
    }

    serial_print("ATA:   id_raw: polling BSY clear...\n");
    int rc = ata_poll_bsy_clear();
    serial_print("ATA:   id_raw: poll_bsy_clear rc=");
    serial_print_dec((uint64_t)(-rc));
    serial_print("\n");
    if (rc != 0) return rc;

    serial_print("ATA:   id_raw: polling DRQ...\n");
    rc = ata_poll_drq();
    serial_print("ATA:   id_raw: poll_drq rc=");
    serial_print_dec((uint64_t)(-rc));
    serial_print("\n");
    if (rc != 0) return rc;

    serial_print("ATA:   id_raw: reading 256 words\n");
    for (int i = 0; i < 256; i++) {
        out[i] = inw(ATA_PRIMARY_DATA);
    }
    serial_print("ATA:   id_raw: read complete\n");
    return 0;
}

int ata_identify(uint8_t drive, uint16_t* out) {
    if (drive > ATA_DRIVE_SLAVE) return -1;
    return ata_identify_raw(drive, out);
}

/* ------------------------------------------------------------------ *
 * Model string extraction
 * ------------------------------------------------------------------ */

static void ata_extract_model(const uint16_t* id, char* out) {
    for (int i = 0; i < 20; i++) {
        uint16_t w = id[27 + i];
        out[i * 2]     = (char)(w >> 8);
        out[i * 2 + 1] = (char)(w & 0xFF);
    }
    out[40] = '\0';

    for (int i = 39; i >= 0; i--) {
        if (out[i] == ' ') out[i] = '\0';
        else break;
    }
}

static int ata_probe(uint8_t drive) {
    serial_print("ATA: probe drive ");
    serial_print_dec(drive);
    serial_print("\n");

    uint16_t id[256];

    int rc = ata_identify_raw(drive, id);
    serial_print("ATA: probe drive ");
    serial_print_dec(drive);
    serial_print(" id_raw rc=");
    serial_print_dec((uint64_t)(-rc));
    serial_print("\n");

    if (rc == -1) {
        serial_print("ATA: probe drive ");
        serial_print_dec(drive);
        serial_print(": no device\n");
        return 0;
    }
    if (rc == -2) {
        serial_print("ATA: probe drive ");
        serial_print_dec(drive);
        serial_print(": ATAPI (skipped)\n");
        return 0;
    }
    if (rc != 0) {
        serial_print("ATA: probe drive ");
        serial_print_dec(drive);
        serial_print(": IDENTIFY failed\n");
        return 0;
    }

    serial_print("ATA: probe drive ");
    serial_print_dec(drive);
    serial_print(": extracting model\n");
    ata_extract_model(id, s_model[drive]);
    serial_print("ATA: probe drive ");
    serial_print_dec(drive);
    serial_print(": model = ");
    serial_print(s_model[drive]);
    serial_print("\n");
    return 1;
}

int ata_init(void) {
    serial_print("ATA: probing primary channel...\n");

    s_present[ATA_DRIVE_MASTER] = ata_probe(ATA_DRIVE_MASTER);
    if (s_present[ATA_DRIVE_MASTER]) {
        serial_print("ATA:   master: ");
        serial_print(s_model[ATA_DRIVE_MASTER]);
        serial_print("\n");
    }

    s_present[ATA_DRIVE_SLAVE] = ata_probe(ATA_DRIVE_SLAVE);
    if (s_present[ATA_DRIVE_SLAVE]) {
        serial_print("ATA:   slave:  ");
        serial_print(s_model[ATA_DRIVE_SLAVE]);
        serial_print("\n");
    }

    if (s_present[ATA_DRIVE_MASTER]) {
        s_default_drive = ATA_DRIVE_MASTER;
    } else if (s_present[ATA_DRIVE_SLAVE]) {
        s_default_drive = ATA_DRIVE_SLAVE;
        serial_print("ATA: master absent, using slave as default\n");
    } else {
        serial_print("ATA: no devices on primary channel\n");
        return -1;
    }

    serial_print("ATA: ready, default drive = ");
    serial_print_dec(s_default_drive);
    serial_print("\n");
    return 0;
}

int ata_present(uint8_t drive) {
    if (drive > ATA_DRIVE_SLAVE) return 0;
    return s_present[drive];
}

const char* ata_model(uint8_t drive) {
    if (drive > ATA_DRIVE_SLAVE) return 0;
    if (!s_present[drive]) return 0;
    return s_model[drive];
}

uint32_t ata_get_sector_count(uint8_t drive) {
    if (drive > ATA_DRIVE_SLAVE) return 0;
    if (!s_present[drive]) return 0;

    uint16_t id[256];
    if (ata_identify(drive, id) != 0) return 0;

    /*
     * Words 60-61 hold the 28-bit LBA max sector count.
     * Words 100-103 hold the 48-bit value. We're LBA28-only, so use
     * 60-61. The value is the number of addressable sectors, so the
     * highest valid LBA is (count - 1).
     */
    uint32_t count = ((uint32_t)id[61] << 16) | id[60];
    return count;
}

/* ------------------------------------------------------------------ *
 * READ
 * ------------------------------------------------------------------ */

static int ata_read_chunk(uint8_t drive, uint32_t lba, uint8_t count, void* buf) {
    serial_print("ATA: read_chunk lba=");
    serial_print_dec(lba);
    serial_print(" count=");
    serial_print_dec(count);
    serial_print("\n");

    int rc = ata_poll_bsy_clear();
    if (rc != 0) {
        serial_print("ATA: read_chunk: poll_bsy fail rc=");
        serial_print_dec((uint64_t)(-rc));
        serial_print("\n");
        return rc;
    }

    ata_select_drive(drive, lba);
    serial_print("ATA: read_chunk: drive selected\n");

    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO,   (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI,   (uint8_t)((lba >> 16) & 0xFF));
    serial_print("ATA: read_chunk: LBA programmed\n");

    outb(ATA_PRIMARY_STATUS, ATA_CMD_READ_SECTORS);
    ata_400ns_delay();

    uint8_t st = inb(ATA_PRIMARY_STATUS);
    serial_print("ATA: read_chunk: status after cmd = 0x");
    serial_print_hex(st);
    serial_print("\n");

    uint16_t* p = (uint16_t*)buf;
    uint32_t sectors = (count == 0) ? 256 : count;

    for (uint32_t s = 0; s < sectors; s++) {
        rc = ata_poll_drq();
        if (rc != 0) {
            serial_print("ATA: read_chunk: poll_drq fail on sector ");
            serial_print_dec(s);
            serial_print(" rc=");
            serial_print_dec((uint64_t)(-rc));
            serial_print("\n");
            return rc;
        }
        for (int i = 0; i < 256; i++) {
            *p++ = inw(ATA_PRIMARY_DATA);
        }
        ata_400ns_delay();
    }

    serial_print("ATA: read_chunk: done\n");
    return 0;
}

/* ------------------------------------------------------------------ *
 * WRITE
 * ------------------------------------------------------------------ */

static int ata_write_chunk(uint8_t drive, uint32_t lba, uint8_t count,
                           const void* buf) {
    int rc = ata_poll_bsy_clear();
    if (rc != 0) return rc;

    ata_select_drive(drive, lba);

    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO,   (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI,   (uint8_t)((lba >> 16) & 0xFF));

    outb(ATA_PRIMARY_STATUS, ATA_CMD_WRITE_SECTORS);
    ata_400ns_delay();

    const uint16_t* p = (const uint16_t*)buf;
    uint32_t sectors = (count == 0) ? 256 : count;

    for (uint32_t s = 0; s < sectors; s++) {
        rc = ata_poll_drq();
        if (rc != 0) return rc;
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, *p++);
        }
        ata_400ns_delay();
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Drive-parameterized API
 * ------------------------------------------------------------------ */

int ata_read_sector_drive(uint8_t drive, uint32_t lba, void* buf) {
    if (drive > ATA_DRIVE_SLAVE) return -1;
    if (!s_present[drive]) return -1;
    return ata_read_chunk(drive, lba, 1, buf);
}

int ata_read_sectors_drive(uint8_t drive, uint32_t lba, uint32_t count, void* buf) {
    if (drive > ATA_DRIVE_SLAVE) return -1;
    if (!s_present[drive]) return -1;
    if (count == 0) return 0;

    uint8_t* p = (uint8_t*)buf;
    while (count > 0) {
        uint32_t chunk = (count > 256) ? 256 : count;
        uint8_t sc = (chunk == 256) ? 0 : (uint8_t)chunk;

        int rc = ata_read_chunk(drive, lba, sc, p);
        if (rc != 0) return rc;

        lba   += chunk;
        count -= chunk;
        p     += chunk * ATA_SECTOR_SIZE;
    }
    return 0;
}

int ata_write_sector_drive(uint8_t drive, uint32_t lba, const void* buf) {
    return ata_write_sectors_drive(drive, lba, 1, buf);
}

int ata_write_sectors_drive(uint8_t drive, uint32_t lba, uint32_t count, const void* buf) {
    if (drive > ATA_DRIVE_SLAVE) return -1;
    if (!s_present[drive]) return -1;
    if (count == 0) return 0;

    /*
     * The write-protect floor protects the boot chain, the kernel,
     * and the gap before the FAT partition on the master disk. It
     * does not apply to the slave (dual-drive mode's fat.img), which
     * starts at LBA 0 and holds only a filesystem.
     */
    if (drive == ATA_DRIVE_MASTER && lba < ATA_WRITE_PROTECT_LBAS) {
        serial_print("ATA: refuse write below LBA ");
        serial_print_dec(ATA_WRITE_PROTECT_LBAS);
        serial_print(" on master (requested LBA ");
        serial_print_dec(lba);
        serial_print(")\n");
        return -2;
    }
    if (lba + count <= lba) return -1;

    const uint8_t* p = (const uint8_t*)buf;
    while (count > 0) {
        uint32_t chunk = (count > 256) ? 256 : count;
        uint8_t sc = (chunk == 256) ? 0 : (uint8_t)chunk;

        int rc = ata_write_chunk(drive, lba, sc, p);
        if (rc != 0) return rc;

        lba   += chunk;
        count -= chunk;
        p     += chunk * ATA_SECTOR_SIZE;
    }

    /* FLUSH CACHE on the same drive we just wrote to. */
    int rc = ata_poll_bsy_clear();
    if (rc != 0) return rc;
    outb(ATA_PRIMARY_STATUS, ATA_CMD_FLUSH_CACHE);
    ata_400ns_delay();
    return ata_poll_bsy_clear();
}

int ata_flush_cache_drive(uint8_t drive) {
    if (drive > ATA_DRIVE_SLAVE) return -1;
    if (!s_present[drive]) return -1;

    /*
     * FLUSH CACHE is not LBA-addressed; it flushes whatever drive is
     * currently selected. Re-select via a dummy 0-LBA select so we
     * flush the right device.
     */
    ata_select_drive(drive, 0);

    int rc = ata_poll_bsy_clear();
    if (rc != 0) return rc;

    outb(ATA_PRIMARY_STATUS, ATA_CMD_FLUSH_CACHE);
    ata_400ns_delay();
    return ata_poll_bsy_clear();
}

int ata_read_sector(uint32_t lba, void* buf) {
    return ata_read_sector_drive(s_default_drive, lba, buf);
}

int ata_read_sectors(uint32_t lba, uint32_t count, void* buf) {
    return ata_read_sectors_drive(s_default_drive, lba, count, buf);
}

int ata_write_sector(uint32_t lba, const void* buf) {
    return ata_write_sector_drive(s_default_drive, lba, buf);
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buf) {
    return ata_write_sectors_drive(s_default_drive, lba, count, buf);
}

int ata_flush_cache(void) {
    return ata_flush_cache_drive(s_default_drive);
}
