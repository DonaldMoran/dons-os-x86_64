/* 04_kernel_64bit/fatfs/diskio.c */
#include "ff.h"
#include "diskio.h"
#include "../include/ata.h"
#include "../include/fat_config.h"

/* Map physical drive 0 to the configured FatFs drive */
#define DEV_FAT 0

/*
 * Partition offset. In single-drive mode the FAT volume starts at
 * LBA 2048 of the master disk, and FF_MULTI_PARTITION is 0 in
 * ffconf.h, so FatFs is NOT partition-aware: it treats the volume as
 * starting at LBA 0 of the physical drive. diskio.c must translate
 * every FatFs sector number (which is volume-relative) into a device
 * LBA by adding this offset.
 *
 * In dual-drive mode the FAT volume is a whole-disk image (fat.img)
 * with no partition table, so the offset is 0.
 */
#if FAT_CONFIG_SINGLE_DRIVE
    #define FAT_PARTITION_OFFSET  2048
#else
    #define FAT_PARTITION_OFFSET  0
#endif

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != DEV_FAT) return STA_NOINIT;

    if (!ata_present(FAT_DRIVE)) {
        return STA_NODISK;
    }
    return 0;
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != DEV_FAT) return STA_NOINIT;

    if (!ata_present(FAT_DRIVE)) {
        return STA_NODISK;
    }
    return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_FAT || count == 0) return RES_PARERR;

    uint32_t dev_lba = (uint32_t)sector + FAT_PARTITION_OFFSET;

    int rc = ata_read_sectors_drive(FAT_DRIVE, dev_lba,
                                    (uint32_t)count, buff);
    return (rc == 0) ? RES_OK : RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_FAT || count == 0) return RES_PARERR;

    uint32_t dev_lba = (uint32_t)sector + FAT_PARTITION_OFFSET;

    int rc = ata_write_sectors_drive(FAT_DRIVE, dev_lba,
                                     (uint32_t)count, buff);
    return (rc == 0) ? RES_OK : RES_ERROR;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != DEV_FAT) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            if (ata_flush_cache_drive(FAT_DRIVE) != 0) {
                return RES_ERROR;
            }
            return RES_OK;

        case GET_SECTOR_COUNT:
            /*
             * Return the size of the VOLUME, not the physical device.
             * FatFs uses this for f_getfree() and cluster accounting.
             */
            *(LBA_t*)buff = FAT_VOLUME_SECTORS;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = ATA_SECTOR_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
    }

    return RES_PARERR;
}

#include <stdint.h>

uint32_t get_fattime(void) {
    /* Static fallback: Jan 1, 2026 00:00:00 */
    return ((uint32_t)(2026 - 1980) << 25) |
           ((uint32_t)1 << 21)              |
           ((uint32_t)1 << 16)              |
           ((uint32_t)0 << 11)              |
           ((uint32_t)0 << 5)               |
           ((uint32_t)0 >> 1);
}
