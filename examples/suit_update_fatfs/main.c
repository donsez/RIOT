/*
 * Copyright (C) 2019 Kaspar Schleiser <kaspar@schleiser.de>
 *               2021 Université Grenoble Alpes
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       SUIT updates over CoAP example server application (using nanocoap)
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Didier Donsez <didier.donsez@univ-grenoble-alpes.fr>
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "thread.h"
#include "irq.h"
#include "xtimer.h"

#include "shell.h"

#include "suit/transport/fatfs.h"
#include "riotboot/slot.h"

#ifdef MODULE_PERIPH_GPIO
#include "periph/gpio.h"
#endif

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

/* assuming that first button is always BTN0 */
#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void cb(void *arg)
{
    (void) arg;
    printf("Button pressed! Triggering suit update! \n");
    suit_fatfs_trigger((uint8_t *) SUIT_MANIFEST_RESOURCE, sizeof(SUIT_MANIFEST_RESOURCE));
}
#endif


static int cmd_print_slot_nr(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("Current slot=%d\n", riotboot_slot_current());
    return 0;
}

static int cmd_print_slot_hdr(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int current_slot = riotboot_slot_current();
    riotboot_slot_print_hdr(current_slot);
    return 0;
}

static int cmd_print_slot_addr(int argc, char **argv)
{
    (void)argc;

    int reqslot=atoi(argv[1]);
    printf("Slot %d address=0x%08" PRIx32 "\n",
           reqslot, riotboot_slot_get_image_startaddr(reqslot));
    return 0;
}

static int cmd_dumpaddrs(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    riotboot_slot_dump_addrs();
    return 0;
}

static int cmd_suit_update(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("Triggering suit update! \n");
    suit_fatfs_trigger((uint8_t *) SUIT_MANIFEST_RESOURCE, sizeof(SUIT_MANIFEST_RESOURCE));
    return 0;
}

static const shell_command_t shell_commands[] = {
	{ "update", "Trigger suit update", cmd_suit_update },
	{ "curslotnr", "Print current slot number", cmd_print_slot_nr },
    { "curslothdr", "Print current slot header", cmd_print_slot_hdr },
    { "getslotaddr", "Print address of requested slot", cmd_print_slot_addr },
    { "dumpaddrs", "Prints all slot data in header", cmd_dumpaddrs },
    { NULL, NULL, NULL }
};


int main(void)
{
    puts("RIOT SUIT update example application");

    int current_slot = riotboot_slot_current();
    if (current_slot != -1) {
        /* Sometimes, udhcp output messes up the following printfs.  That
         * confuses the test script. As a workaround, just disable interrupts
         * for a while.
         */
        irq_disable();
        printf("running from slot %d\n", current_slot);
        printf("slot start addr = %p\n", (void *)riotboot_slot_get_hdr(current_slot));
        riotboot_slot_print_hdr(current_slot);
        irq_enable();
    }
    else {
        printf("[FAILED] You're not running riotboot\n");
    }

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
    /* initialize a button to manually trigger an update */
    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_FALLING, cb, NULL);
#endif

    /* init fatfs and mount volume */
    suit_fatfs_init();
    suit_fatfs_mount(0);
    suit_fatfs_ls("FIRMWARE");

    /* start suit fatfs updater thread */
    suit_fatfs_run();

    /* the shell contains commands that receive packets via GNRC and thus
       needs a msg queue */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("Starting the shell");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
