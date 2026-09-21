#ifndef TEST_FF_H
#define TEST_FF_H
/* API-only double for fs_shared policy/error propagation, NOT a FAT model.
 * Actual on-media filesystem behavior is tested on the board. */
#include <stdint.h>
typedef uint8_t BYTE;
typedef unsigned UINT;
typedef uint32_t FSIZE_t;
typedef struct { unsigned unused; } FATFS;
typedef struct { FSIZE_t size; } FIL;
typedef struct { unsigned unused; } DIR;
typedef struct { char fname[256], altname[13]; BYTE fattrib; FSIZE_t fsize; } FILINFO;
typedef enum {
    FR_OK, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE, FR_NO_PATH,
    FR_INVALID_NAME, FR_DENIED, FR_EXIST, FR_INVALID_OBJECT, FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE, FR_NOT_ENABLED, FR_NO_FILESYSTEM, FR_MKFS_ABORTED,
    FR_TIMEOUT, FR_LOCKED, FR_NOT_ENOUGH_CORE, FR_TOO_MANY_OPEN_FILES, FR_INVALID_PARAMETER
} FRESULT;
#define FF_USE_LFN 3
#define FF_MAX_LFN 255
#define FF_MAX_SS 512
#define FM_ANY 7
#define FM_FAT32 2
#define FM_SFD 8
#define AM_DIR 0x10
#define FA_READ 1
#define FA_WRITE 2
#define FA_CREATE_ALWAYS 8
#define FA_OPEN_ALWAYS 16
#define f_size(fp) ((fp)->size)
FRESULT f_mount(FATFS *, const char *, BYTE);
FRESULT f_mkfs(const char *, BYTE, unsigned, void *, UINT);
FRESULT f_open(FIL *, const char *, BYTE);
FRESULT f_close(FIL *);
FRESULT f_truncate(FIL *);
FRESULT f_lseek(FIL *, FSIZE_t);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_mkdir(const char *);
FRESULT f_unlink(const char *);
FRESULT f_rename(const char *, const char *);
FRESULT f_stat(const char *, FILINFO *);
FRESULT f_opendir(DIR *, const char *);
FRESULT f_readdir(DIR *, FILINFO *);
FRESULT f_closedir(DIR *);
#endif
