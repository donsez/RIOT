/*
 * Copyright (C) 2019 Kaspar Schleiser <kaspar@schleiser.de>
 *               2019 Inria
 *               2019 Freie Universität Berlin
 *               2021 Université Grenoble Alpes
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_suit
 * @defgroup    sys_suit_transport_fatfs SUIT firmware FATFS transport
 * @brief       SUIT secure firmware updates over FATFS SDCard
 *
 * @{
 *
 * @brief       SUIT CoAP helper API
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Francisco Molina <francois-xavier.molina@inria.fr>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @author      Didier Donsez <didier.donsez@univ-grenoble-alpes.fr>
 */

#ifndef SUIT_TRANSPORT_FATFS_H
#define SUIT_TRANSPORT_FATFS_H


#ifdef __cplusplus
extern "C" {
#endif

#ifndef FATFS_READ_BUFFER_SIZE
#define FATFS_READ_BUFFER_SIZE		(512)
#endif

/**
 * @brief    Init the SDCard drivers
 */
int suit_fatfs_init(void);

/**
 * @brief    Mount a SDCard volume
 */
int suit_fatfs_mount(int vol_idx);


/**
 * @brief    List the files in the directory
 */
int suit_fatfs_ls(char *dirpath);

/**
 * @brief    Start SUIT CoAP thread
 */
void suit_fatfs_run(void);

/*
 * Dear Reviewer,
 *
 * At the time of PR'ing this code, there was a pile of CoAP PR's waiting for
 * reviews.  Some of that functionality is needed in one way or another for
 * SUIT. In order to not block software updates with CoAP refactoring, some of
 * the work-in-progress code has been copied here.  We expect this to be
 * removed as soon as CoAP in master provides similar functionality.
 *
 * As this is internal code that will go soon, I exclude this from Doxygen.
 *
 * Kaspar (July 2019)
 */
#ifndef DOXYGEN


/**
 * @brief   Coap blockwise request callback descriptor
 *
 * @param[in] arg      Pointer to be passed as arguments to the callback
 * @param[in] offset   Offset of received data
 * @param[in] buf      Pointer to the received data
 * @param[in] len      Length of the received data
 * @param[in] more     -1 for no option, 0 for last block, 1 for more blocks
 *
 * @returns    0       on success
 * @returns   -1       on error
 */
typedef int (*fatfs_blockwise_cb_t)(void *arg, size_t offset, uint8_t *buf, size_t len, int more);

/**
 * @brief    Performs a blockwise fatfs get request to the specified url.
 *
 * This function will fetch the content of the specified resource path via
 * block-wise-transfer. A fatfs_blockwise_cb_t will be called on each received
 * block.
 *
 * @param[in]   url        url pointer to source path
 * @param[in]   blksize    sender suggested SZX for the COAP block request
 * @param[in]   callback   callback to be executed on each received block
 * @param[in]   arg        optional function arguments
 *
 * @returns     -EINVAL    if an invalid url is provided
 * @returns     -1         if failed to fetch the url content
 * @returns      0         on success
 */
int suit_fatfs_get_blockwise_url(const char *url,
		size_t blksize, fatfs_blockwise_cb_t callback, void *arg);

/**
 * @brief   Trigger a SUIT udate
 *
 * @param[in] url       url pointer containing the full fatfs url to the manifest
 * @param[in] len       length of the url
 */
void suit_fatfs_trigger(const uint8_t *url, size_t len);

#endif /* DOXYGEN */

#ifdef __cplusplus
}
#endif

#endif /* SUIT_TRANSPORT_FATFS_H */
/** @} */
