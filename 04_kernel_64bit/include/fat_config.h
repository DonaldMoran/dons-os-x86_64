#ifndef FAT_CONFIG_H
#define FAT_CONFIG_H

/*
 * FatFs drive configuration for dons-os.
 *
 * The kernel supports two disk layouts:
 *
 *   DUAL-DRIVE (FAT_CONFIG_SINGLE_DRIVE = 0)
 *     hdd.img (master): boot chain at LBA 0..127, kernel at LBA 128+
 *     fat.img (slave) : FAT16 volume, whole-disk, no partition table
 *     FatFs talks to ATA_DRIVE_SLAVE with a partition offset of 0.
 *
 *   SINGLE-DRIVE (FAT_CONFIG_SINGLE_DRIVE = 1)
 *     hdd.img (master): boot chain at LBA 0..127, kernel at LBA 128..2047,
 *                       FAT16 partition at LBA 2048+
 *     FatFs talks to ATA_DRIVE_MASTER with a partition offset of 2048.
 *
 * IMPORTANT: FF_MULTI_PARTITION is 0 in ffconf.h. FatFs is NOT
 * partition-aware: it treats the FAT volume as starting at LBA 0 of
 * the physical drive and does NOT consult the partition table. So
 * diskio.c must translate every volume-relative sector number FatFs
 * hands it into a device LBA by adding the partition offset (see
 * FAT_PARTITION_OFFSET in diskio.c).
 *
 * This is why the offset lives in diskio.c, not in the BPB: FatFs
 * never looks at the BPB's hidden_sectors field when
 * FF_MULTI_PARTITION is 0.
 */

#ifndef FAT_CONFIG_SINGLE_DRIVE
#define FAT_CONFIG_SINGLE_DRIVE 0
#endif

#include "ata.h"

#if FAT_CONFIG_SINGLE_DRIVE
    #define FAT_DRIVE  ATA_DRIVE_MASTER
#else
    #define FAT_DRIVE  ATA_DRIVE_SLAVE
#endif

/*
 * Sector accounting.
 *
 * The disk image is 20 MiB = 40960 sectors.
 * In single-drive mode the FAT partition starts at LBA 2048, so the
 * volume spans 40960 - 2048 = 38912 sectors.
 * In dual-drive mode fat.img is a whole-disk volume: 40960 sectors.
 *
 * These constants are COUPLED to the image-build rules in
 * 05_boot_kernel64/Makefile. If you resize the image or move the
 * partition, update BOTH the Makefile and these values.
 */
#define FAT_DISK_TOTAL_SECTORS   40960
#if FAT_CONFIG_SINGLE_DRIVE
    #define FAT_VOLUME_SECTORS   (FAT_DISK_TOTAL_SECTORS - 2048)
#else
    #define FAT_VOLUME_SECTORS   (FAT_DISK_TOTAL_SECTORS)
#endif

#endif /* FAT_CONFIG_H */
