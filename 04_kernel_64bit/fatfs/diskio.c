/* 04_kernel_64bit/fatfs/diskio.c */
#include "ff.h"
#include "diskio.h"
#include "../include/ata.h" // Points to your include/ata.h header file

/* Map physical drive 0 to your FatFs setup */
#define DEV_FAT 0 

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != DEV_FAT) return STA_NOINIT;
    
    // Check hardware existence using your driver helper
    if (!ata_present(ATA_DRIVE_SLAVE)) {
        return STA_NODISK;
    }
    return 0; // 0 means OK/Ready
}

/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != DEV_FAT) return STA_NOINIT;
    
    // Your ata_init() already handling checking the physical wire at boot
    if (!ata_present(ATA_DRIVE_SLAVE)) {
        return STA_NODISK;
    }
    return 0; // 0 means Successfully Initialized
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_FAT || count == 0) return RES_PARERR;

    // Direct route to your drive-parameterized multi-sector read helper
    int rc = ata_read_sectors_drive(ATA_DRIVE_SLAVE, (uint32_t)sector, (uint32_t)count, buff);
    
    return (rc == 0) ? RES_OK : RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != DEV_FAT || count == 0) return RES_PARERR;

    // Direct route to your drive-parameterized multi-sector write helper
    int rc = ata_write_sectors_drive(ATA_DRIVE_SLAVE, (uint32_t)sector, (uint32_t)count, buff);
    
    return (rc == 0) ? RES_OK : RES_ERROR;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions (Required for FAT runtime state/sync)        */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != DEV_FAT) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            // Flush the specific slave drive's hardware write caches
            if (ata_flush_cache_drive(ATA_DRIVE_SLAVE) != 0) {
                return RES_ERROR;
            }
            return RES_OK;

        case GET_SECTOR_COUNT:
            // Match your fat.img sizing geometry (40960 sectors for 20MB)
            *(LBA_t*)buff = 40960; 
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = ATA_SECTOR_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; // Non-flash layout / standard erase blocks
            return RES_OK;
    }

    return RES_PARERR;
}

#include <stdint.h>

uint32_t get_fattime(void) {
    /* Return a static fallback timestamp: Jan 1, 2026 00:00:00 */
    return ((uint32_t)(2026 - 1980) << 25) | // Year
           ((uint32_t)1 << 21)              | // Month
           ((uint32_t)1 << 16)              | // Day
           ((uint32_t)0 << 11)              | // Hour
           ((uint32_t)0 << 5)               | // Min
           ((uint32_t)0 >> 1);                // Sec
}
