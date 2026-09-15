#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

#define ATA_DRIVE_MASTER 0
#define ATA_DRIVE_SLAVE  1

/*
 * Drive-parameterized read/write. Same semantics as the non-drive
 * variants above, but operate on the given drive instead of the
 * default. Intended for callers that need a specific device (e.g.
 * FatFs on the slave disk).
 *
 * Returns 0 on success, negative on error, or -1 if the drive is not
 * present.
 */
int ata_read_sector_drive(uint8_t drive, uint32_t lba, void* buf);
int ata_read_sectors_drive(uint8_t drive, uint32_t lba, uint32_t count, void* buf);
int ata_write_sector_drive(uint8_t drive, uint32_t lba, const void* buf);
int ata_write_sectors_drive(uint8_t drive, uint32_t lba, uint32_t count, const void* buf);
int ata_flush_cache_drive(uint8_t drive);

/*
 * Initialize the ATA driver.
 *
 * Probes primary master and primary slave. If a device is found, its model
 * string is read via IDENTIFY and logged to serial. Master is the default
 * device for all read/write calls. Returns 0 if at least one device was
 * found on the primary channel, negative if none.
 *
 * Assumes QEMU's default PATA compatibility-mode attachment:
 *     -drive file=hdd.img,format=raw
 * which places the disk on the primary channel at I/O base 0x1F0 with
 * control at 0x3F6. If hdd.img is ever attached as virtio, SCSI, or
 * AHCI, this driver will not find it.
 */
int ata_init(void);

/* 1 if the given drive was detected during ata_init, 0 otherwise. */
int ata_present(uint8_t drive);

/* Model string for the given drive, or NULL if not present. */
const char* ata_model(uint8_t drive);

/*
 * Read a single 512-byte sector into buf.
 * buf must be at least 512 bytes and should be 2-byte aligned.
 * Returns 0 on success, negative on error.
 */
int ata_read_sector(uint32_t lba, void* buf);

/*
 * Read count consecutive sectors starting at lba.
 * buf must be at least count * 512 bytes.
 * count is clamped to 256 per hardware command; larger counts are split
 * into multiple commands internally.
 * Returns 0 on success, negative on error.
 */
int ata_read_sectors(uint32_t lba, uint32_t count, void* buf);

/*
 * Write a single 512-byte sector from buf.
 * buf must be at least 512 bytes and should be 2-byte aligned.
 * Refuses to write below ATA_WRITE_PROTECT_LBAS (see ata.c) unless the
 * internal force path is used, which this API does not expose.
 * Returns 0 on success, negative on error.
 */
int ata_write_sector(uint32_t lba, const void* buf);

/*
 * Write count consecutive sectors starting at lba.
 * buf must be at least count * 512 bytes.
 * Issues a FLUSH CACHE after the last sector so the data is durable.
 * Returns 0 on success, negative on error.
 */
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buf);

/*
 * Issue FLUSH CACHE (0xE7) to the default drive and poll until it
 * completes. Returns 0 on success, negative on error.
 */
int ata_flush_cache(void);

/*
 * Run IDENTIFY DEVICE (0xEC) against the given drive and copy the 256
 * returned words into out. Intended for diagnostics; ata_init calls the
 * same underlying routine internally.
 * Returns 0 on success, negative on error.
 */
int ata_identify(uint8_t drive, uint16_t* out);

#endif
