/*
 * Copyright (C) 2019 Freie Universität Berlin
 *               2019 Inria
 *               2019 Kaspar Schleiser <kaspar@schleiser.de>
 *               2021 Université Grenoble Alpes
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_suit
 * @{
 *
 * @file
 * @brief       SUIT fatfs
 *
 * @author      Koen Zandberg <koen@bergzand.net>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Francisco Molina <francois-xavier.molina@inria.fr>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @author      Didier Donsez <didier.donsez@univ-grenoble-alpes.fr>
 * @}
 */

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "msg.h"
#include "log.h"
#include "thread.h"
#include "periph/pm.h"
#include "xtimer.h"

#include "suit/transport/fatfs.h"

#ifdef MODULE_RIOTBOOT_SLOT
#include "riotboot/slot.h"
#endif

#ifdef MODULE_SUIT
#include "suit.h"
#include "suit/handlers.h"
#include "suit/storage.h"
#endif

#if defined(MODULE_PROGRESS_BAR)
#include "progress_bar.h"
#endif

#define ENABLE_DEBUG 0
#include "debug.h"

#ifndef SUIT_FATFS_STACKSIZE
/* allocate stack needed to do manifest validation */
#define SUIT_FATFS_STACKSIZE (3 * THREAD_STACKSIZE_LARGE)
#endif

#ifndef SUIT_FATFS_PRIO
#define SUIT_FATFS_PRIO THREAD_PRIORITY_MAIN - 1
#endif

#ifndef SUIT_URL_MAX
#define SUIT_URL_MAX            128
#endif

#ifndef SUIT_MANIFEST_BUFSIZE
#define SUIT_MANIFEST_BUFSIZE   640
#endif

#define SUIT_MSG_TRIGGER        0x12345

static char _stack[SUIT_FATFS_STACKSIZE];
static char _url[SUIT_URL_MAX];
static uint8_t _manifest_buf[SUIT_MANIFEST_BUFSIZE];

#ifdef MODULE_SUIT
static inline void _print_download_progress(suit_manifest_t *manifest,
                                            size_t offset, size_t len,
                                            size_t image_size)
{
    (void)manifest;
    (void)offset;
    (void)len;
    DEBUG("_suit_flashwrite(): writing %u bytes at pos %u\n", len, offset);
#if defined(MODULE_PROGRESS_BAR)
    if (image_size != 0) {
        char _suffix[7] = { 0 };
        uint8_t _progress = 100 * (offset + len) / image_size;
        sprintf(_suffix, " %3d%%", _progress);
        progress_bar_print("Fetching firmware ", _suffix, _progress);
        if (_progress == 100) {
            puts("");
        }
    }
#endif
}
#endif

static kernel_pid_t _suit_fatfs_pid;



#if FATFS_FFCONF_OPT_FS_NORTC == 0
#include "periph/rtc.h"
#endif
#include "mtd.h"
#include "fatfs_diskio_mtd.h"
#include "fatfs/ff.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#define TEST_FATFS_MAX_LBL_SIZE 64
#define TEST_FATFS_MAX_VOL_STR_LEN 14 /* "-2147483648:/\0" */
#define TEST_FATFS_FIXED_SECTOR_SIZE 512
#define TEST_FATFS_FATENT_OFFSET 2
#define TEST_FATFS_SHIFT_B_TO_GIB 30
#define TEST_FATFS_SHIFT_B_TO_MIB 20
#define TEST_FATFS_RTC_MON_OFFSET 1
#define TEST_FATFS_RTC_YEAR 2000
#define TEST_FATFS_RTC_MON  1
#define TEST_FATFS_RTC_DAY  1
#define TEST_FATFS_RTC_H    0
#define TEST_FATFS_RTC_M    0
#define TEST_FATFS_RTC_S    0
#define IEC_KIBI 1024
#define SI_KILO 1000


static FATFS fat_fs; /* FatFs work area needed for each volume */


#ifdef MODULE_MTD_NATIVE
/* mtd device for native is provided in boards/native/board_init.c */
extern mtd_dev_t *mtd0;
mtd_dev_t *fatfs_mtd_devs[1];
#elif MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi_params.h"
#define SDCARD_SPI_NUM ARRAY_SIZE(sdcard_spi_params)
/* sdcard devs are provided by drivers/sdcard_spi/sdcard_spi.c */
extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];
mtd_sdcard_t mtd_sdcard_devs[SDCARD_SPI_NUM];
mtd_dev_t *fatfs_mtd_devs[SDCARD_SPI_NUM];
#endif

static bool is_init = false;

int suit_fatfs_init(void) {

#if MODULE_MTD_SDCARD
	if(!is_init) {
		for (unsigned int i = 0; i < SDCARD_SPI_NUM; i++){
			mtd_sdcard_devs[i].base.driver = &mtd_sdcard_driver;
			mtd_sdcard_devs[i].sd_card = &sdcard_spi_devs[i];
			mtd_sdcard_devs[i].params = &sdcard_spi_params[i];
			fatfs_mtd_devs[i] = &mtd_sdcard_devs[i].base;

			if(mtd_init(&mtd_sdcard_devs[i].base) == 0) {
				printf("init: sdcard_mtd %u [OK]\n", i);
			} else {
				printf("init: sdcard_mtd %u [FAILED]\n", i);
			}
		}
		is_init = true;
	}
	return 0;
#else
	return 0;
#endif
}


#define MTD_NUM ARRAY_SIZE(fatfs_mtd_devs)

int suit_fatfs_mount(int vol_idx) {

	if (vol_idx > (int) (MTD_NUM - 1)) {
		printf("mount: max allowed <volume_idx> is %d\n", (int) (MTD_NUM - 1));
		return -1;
	}

	char volume_str[TEST_FATFS_MAX_VOL_STR_LEN];
	sprintf(volume_str, "%d:/", vol_idx);

	DEBUG("mount: mounting file system image...");

	/* "0:/" points to the root dir of drive 0 */
	FRESULT mountresu = f_mount(&fat_fs, volume_str, 1);
	TCHAR label[TEST_FATFS_MAX_LBL_SIZE];

	if (mountresu == FR_OK) {
		DEBUG("mount: [OK]");
		if (f_getlabel("", label, NULL) == FR_OK) {
			printf("mount: Volume name: %s\n", label);
		}

		FATFS *fs;
		DWORD fre_clust;

		/* Get volume information and free clusters of selected drive */
		if (f_getfree(volume_str, &fre_clust, &fs) != FR_OK) {
			puts("mount: wasn't able to get volume size info!");
		} else {

#if FF_MAX_SS == FF_MIN_SS
			uint16_t sector_size = TEST_FATFS_FIXED_SECTOR_SIZE;
#else
            uint16_t sector_size = fs->ssize;
            #endif

			uint64_t total_bytes = (fs->n_fatent - TEST_FATFS_FATENT_OFFSET)
					* fs->csize;
			total_bytes *= sector_size;
			uint64_t free_bytes = fre_clust * fs->csize;
			free_bytes *= sector_size;

			uint32_t to_gib_i = total_bytes >> TEST_FATFS_SHIFT_B_TO_GIB;
			uint32_t to_gib_f = ((((total_bytes >> TEST_FATFS_SHIFT_B_TO_MIB)
					- to_gib_i * IEC_KIBI) * SI_KILO) / IEC_KIBI);

			uint32_t fr_gib_i = free_bytes >> TEST_FATFS_SHIFT_B_TO_GIB;
			uint32_t fr_gib_f = ((((free_bytes >> TEST_FATFS_SHIFT_B_TO_MIB)
					- fr_gib_i * IEC_KIBI) * SI_KILO) / IEC_KIBI);

			printf("mount: %" PRIu32 ",%03" PRIu32 " GiB of %" PRIu32 ",%03" PRIu32
			" GiB available\n", fr_gib_i, fr_gib_f, to_gib_i, to_gib_f);
		}
	} else {
		puts("mount: [FAILED]");
		switch (mountresu) {
		case FR_NO_FILESYSTEM:
			puts("mount: no filesystem -> you need to format the card to FAT");
			break;
		case FR_DISK_ERR:
			puts("mount: error in the low-level disk driver!");
			break;
		default:
			printf("mount: error %d -> see ff.h of fatfs package for "
					"further details\n", mountresu);
		}
		return -1;
	}
	return 0;
}


int suit_fatfs_ls(char *dirpath) {
	FRESULT res;
	DIR dir;
	static FILINFO fno;

	if (dirpath == NULL) {
		dirpath = "/";
	}

	res = f_opendir(&dir, dirpath);/* Open the directory */

	if (res == FR_OK) {
		while (true) {
			res = f_readdir(&dir, &fno); /* Read a directory item */

			if ((res != FR_OK) || fno.fname[0] == 0) {
				break; /* Break on error or end of dir */
			}

			if (fno.fattrib & AM_DIR) { /* if this element is a directory */
				printf("%s%s/\n", dirpath, fno.fname);
			} else {
				printf("%s/%s\n", dirpath, fno.fname);
			}
		}

		f_closedir(&dir);
		return 0;
	}

	printf("ls: [FAILED] error %d\n", res);
	return -1;
}

static int suit_fatfs_openread(FIL *fd, const char* path, size_t* file_size) {
	FRESULT open_resu = f_open(fd, path, FA_READ | FA_OPEN_EXISTING);
	if (open_resu == FR_OK) {
		*file_size = f_size(fd);
		DEBUG("open: [OK] (f_open %s size=%u)\n", path, *file_size);
		return 0;
	} else {
		printf("open: [FAILED] (f_open %s error=%d)\n", path, open_resu);
		return -1;
	}
}

static int suit_fatfs_close(FIL *fd) {
	FRESULT close_resu = f_close(fd);

	if (close_resu == FR_OK) {
		DEBUG("close: [OK] (f_close)\n");
		return 0;
	} else {
		printf("close: [FAILED] (f_close error=%d)\n", close_resu);
		return -1;
	}
}

static int suit_fatfs_read(FIL *fd, size_t file_size,
		size_t offset, size_t blksize, uint8_t *buf, UINT *read_bytes) {
	if (offset >= file_size) {
		DEBUG("read: [FAILED] offset (%d) is greated or equal to file_size (%d)\n", offset, file_size);
		suit_fatfs_close(fd);
		return -1;
	}

	uint32_t to_read = file_size - offset;

	if (to_read > blksize) {
			to_read = blksize;
	}

	FRESULT lseek_resu = f_lseek(fd, offset);
	if (lseek_resu != FR_OK) {
		printf("read: [FAILED] f_lseek error=%d\n", lseek_resu);
		suit_fatfs_close(fd);
		return -1;
	}
	FRESULT read_resu = f_read(fd, buf, to_read, read_bytes);
	if (read_resu != FR_OK) {
		printf("read: [FAILED] (f_read error=%d)\n", read_resu);
		suit_fatfs_close(fd);
		return -1;
	} else {
		DEBUG("read: [OK] (f_read read_bytes=%u)\n", *read_bytes);
		return 0;
	}
}

static uint32_t get_number_of_blocks(uint32_t file_size, uint32_t blk_size){
	uint32_t number_of_blocks = file_size
			/ (uint32_t) blk_size;
	if (file_size
			% (uint32_t) blk_size
			!= 0) {
		number_of_blocks++;
	}
	return number_of_blocks;
}

static void debug_ba(const uint8_t* ba, const size_t len, const size_t line_size) {
    for (size_t i = 0; i < len; i++) {
    	if(i%line_size == 0) DEBUG("\n");
    	DEBUG("%02x ", ba[i]);
    }
    DEBUG("\n");
}


int suit_fatfs_get_blockwise(const char *path,
							size_t blksize,
                            fatfs_blockwise_cb_t callback, void *arg)
{
	int res;
	uint8_t buf[blksize];
    FIL fd;
    size_t file_size;

    res = suit_fatfs_openread(&fd, path, &file_size);
    if (res) {
    	goto out;
    }

    res = -1;

    uint32_t num_blk = get_number_of_blocks(file_size,blksize);
    for(size_t blockn = 0; blockn < num_blk; blockn++) {
        DEBUG("reading block %u (num_blk=%ld)\n", blockn, num_blk);
        UINT read_bytes;
        size_t offset = blockn*blksize;
        res = suit_fatfs_read(&fd, file_size, offset, blksize, buf, &read_bytes);

        DEBUG("res=%i read=%d\n", res, read_bytes);

        if (!res) {

            if (callback(arg, offset, buf, read_bytes,
                         (blockn < num_blk-1)?1:0)) {
                DEBUG("callback res != 0, aborting.\n");
                suit_fatfs_close(&fd);
                res = -1;
                goto out;
            }
        }
        else {
            DEBUG("error reading block\n");
            res = -1;
            goto out;
        }
    }
    suit_fatfs_close(&fd);

out:
    return res;
}

typedef struct {
    size_t offset;
    uint8_t *ptr;
    size_t len;
} _buf_t;

static int _2buf(void *arg, size_t offset, uint8_t *buf, size_t len, int more)
{
    (void)more;

    _buf_t *_buf = arg;
    if (_buf->offset != offset) {
        return 0;
    }
    if (len > _buf->len) {
        return -1;
    }
    else {
        memcpy(_buf->ptr, buf, len);
        _buf->offset += len;
        _buf->ptr += len;
        _buf->len -= len;
        return 0;
    }
}

int suit_fatfs_get_blockwise_url(const char *url,
                                size_t blksize,
                                fatfs_blockwise_cb_t callback, void *arg)
{
    if (strncmp(url, "fatfs://", 8)) {
        LOG_INFO("suit: URL doesn't start with \"fatfs://\"\n");
        return -EINVAL;
    }

    return suit_fatfs_get_blockwise(url+8, blksize, callback, arg);
}

ssize_t suit_fatfs_get_blockwise_url_buf(const char *url,
        size_t blksize, uint8_t *buf, size_t len)
{
    _buf_t _buf = { .ptr = buf, .len = len };
    int res = suit_fatfs_get_blockwise_url(url, blksize, _2buf, &_buf);

    return (res < 0) ? (ssize_t)res : (ssize_t)_buf.offset;
}

static void _suit_handle_url(const char *url)
{
    LOG_INFO("suit_fatfs: downloading \"%s\"\n", url);
    ssize_t size = suit_fatfs_get_blockwise_url_buf(url, FATFS_READ_BUFFER_SIZE,
                                                   _manifest_buf,
                                                   SUIT_MANIFEST_BUFSIZE);
    if (size >= 0) {
        LOG_INFO("suit_fatfs: got manifest with size %u\n", (unsigned)size);

#ifdef MODULE_SUIT

        debug_ba(_manifest_buf, SUIT_MANIFEST_BUFSIZE, 16);

        suit_manifest_t manifest;
        memset(&manifest, 0, sizeof(manifest));

        manifest.urlbuf = _url;
        manifest.urlbuf_len = SUIT_URL_MAX;

        int res;
        if ((res = suit_parse(&manifest, _manifest_buf, size)) != SUIT_OK) {
            LOG_INFO("suit_parse() failed. res=%i\n", res);
            return;
        }

#endif
        if (res == 0) {
            const riotboot_hdr_t *hdr = riotboot_slot_get_hdr(
                riotboot_slot_other());
            riotboot_hdr_print(hdr);
            xtimer_sleep(1);

            if (riotboot_hdr_validate(hdr) == 0) {
                LOG_INFO("suit_fatfs: rebooting...\n");
                pm_reboot();
            }
            else {
                LOG_INFO("suit_fatfs: update failed, hdr invalid\n ");
            }
        }
    }
    else {
        LOG_INFO("suit_fatfs: error getting manifest\n");
    }
}

int suit_storage_helper(void *arg, size_t offset, uint8_t *buf, size_t len,
                        int more)
{
    suit_manifest_t *manifest = (suit_manifest_t *)arg;

    uint32_t image_size;
    nanocbor_value_t param_size;
    size_t total = offset + len;
    suit_component_t *comp = &manifest->components[manifest->component_current];
    suit_param_ref_t *ref_size = &comp->param_size;

    /* Grab the total image size from the manifest */
    if ((suit_param_ref_to_cbor(manifest, ref_size, &param_size) == 0) ||
            (nanocbor_get_uint32(&param_size, &image_size) < 0)) {
        /* Early exit if the total image size can't be determined */
        return -1;
    }

    if (image_size < offset + len) {
        /* Extra newline at the start to compensate for the progress bar */
        LOG_ERROR(
            "\n_suit_fatfs(): Image beyond size, offset + len=%u, "
            "image_size=%u\n", (unsigned)(total), (unsigned)image_size);
        return -1;
    }

    if (!more && image_size != total) {
        LOG_INFO("Incorrect size received, got %u, expected %u\n",
                 (unsigned)total, (unsigned)image_size);
        return -1;
    }

    _print_download_progress(manifest, offset, len, image_size);

    int res = suit_storage_write(comp->storage_backend, manifest, buf, offset, len);
    if (!more) {
        LOG_INFO("Finalizing payload store\n");
        /* Finalize the write if no more data available */
        res = suit_storage_finish(comp->storage_backend, manifest);
    }
    return res;
}

static void *_suit_fatfs_thread(void *arg)
{
    (void)arg;

    LOG_INFO("suit_fatfs: started.\n");
    msg_t msg_queue[4];
    msg_init_queue(msg_queue, 4);

    _suit_fatfs_pid = thread_getpid();

    msg_t m;
    while (true) {
        msg_receive(&m);
        DEBUG("suit_fatfs: got msg with type %" PRIu32 "\n", m.content.value);
        switch (m.content.value) {
            case SUIT_MSG_TRIGGER:
                LOG_INFO("suit_fatfs: trigger received\n");
                _suit_handle_url(_url);
                break;
            default:
                LOG_WARNING("suit_fatfs: warning: unhandled msg\n");
        }
    }
    return NULL;
}

void suit_fatfs_run(void)
{
    thread_create(_stack, SUIT_FATFS_STACKSIZE, SUIT_FATFS_PRIO,
                  THREAD_CREATE_STACKTEST,
                  _suit_fatfs_thread, NULL, "suit_fatfs");
}

void suit_fatfs_trigger(const uint8_t *url, size_t len)
{
    memcpy(_url, url, len);
    _url[len] = '\0';
    msg_t m = { .content.value = SUIT_MSG_TRIGGER };
    msg_send(&m, _suit_fatfs_pid);
}
