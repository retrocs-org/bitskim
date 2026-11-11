#define FS_INFO 0xf100
#define FS_FAT12 0x10000L
#define FS_FAT16 0x10001L
#define FS_FAT32 0x10002L
#define FS_VFAT12 0x20000L
#define FS_VFAT16 0x20001L
#define FS_VFAT32 0x20002L
struct fs_info{char name[32];long version,type;char type_asc[32];};
