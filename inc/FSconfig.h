#ifndef _FS_CONFIG_H_
#define _FS_CONFIG_H_

#include "HardwareProfile.h"

#define FS_MAX_FILES_OPEN 	1
#define MEDIA_SECTOR_SIZE 	512
//#define ALLOW_FILESEARCH
//#define ALLOW_DIRS
#define SUPPORT_FAT32
#define USERDEFINEDCLOCK

#define MDD_MediaInitialize     USBHostMSDSCSIMediaInitialize
#define MDD_MediaDetect         USBHostMSDSCSIMediaDetect
#define MDD_SectorRead          USBHostMSDSCSISectorRead
#define MDD_SectorWrite         USBHostMSDSCSISectorWrite
#define MDD_InitIO();
#define MDD_ShutdownMedia       USBHostMSDSCSIMediaReset
#define MDD_WriteProtectState   USBHostMSDSCSIWriteProtectState

#endif
