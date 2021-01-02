# Overview

This example shows how to integrate SUIT-compliant firmware updates into a
RIOT application with firmware stored into a SDCard.

It implements basic support of the SUIT architecture using
the manifest format specified in
[draft-ietf-suit-manifest-09](https://tools.ietf.org/id/draft-ietf-suit-manifest-09.txt).

**WARNING**: This code should not be considered production ready for the time being.
             It has not seen much exposure or security auditing.

Table of contents:

- [Prerequisites][prerequisites]
  - [SDCard][prerequisites-sdcard]
- [Setup][setup]
  - [Signing key management][key-management]
- [Tested boards and modules][tested-boards]
- [Perform an update][update]
  - [Build and publish the firmware update][update-build-publish]
  - [Notify an update to the device][update-notify]
- [Detailed explanation][detailed-explanation]
- [Automatic test][test]

## Prerequisites
[prerequisites]: #Prerequisites

- Install python dependencies (only Python3.6 and later is supported):

      $ pip3 install --user ed25519 pyasn1 cbor cryptography cbor2

- (On MacOS) Add the function sha512sum

	 $ function sha512sum() { shasum -a 512 "$@" ; } && export -f sha512sum

- Clone this repository:

      $ git clone https://github.com/RIOT-OS/RIOT
      $ cd RIOT

### SDCard
[prerequisites-sdcard]: #SDCard

Connect the [DFRobot MicroSD module](https://wiki.dfrobot.com/MicroSD_card_module_for_Arduino__SKU_DFR0229_) to the Nucleo board
```
    SD Adapter side 		Nucleo-F411RE			Arduino pinout
    SCK 					PA_5 (SPI1 SCLK)		D13
    MISO 					PA_6 (SPI1 MISO)		D12
    MOSI 					PA_7 (SPI1 MOSI)		D11
    CS 						PB_4 (GPIO SD_CS)		D4
    VCC 					5V						5V
    GND 					GND						GND
```


## Tested boards and modules
[tested-boards]: #TestedBoards

This is the list of tested boards:
* [x] nucleo-f411re
* [x] nucleo-f446re
* [ ] nucleo-f446ze

SDCard Modules
* [DFRobot MicroSD module](https://wiki.dfrobot.com/MicroSD_card_module_for_Arduino__SKU_DFR0229)

## Setup
[setup]: #Setup

### Key Management
[key-management]: #Key-management

SUIT keys consist of a private and a public key file, stored in `$(SUIT_KEY_DIR)`.
Similar to how ssh names its keyfiles, the public key filename equals the
private key file, but has an extra `.pub` appended.

`SUIT_KEY_DIR` defaults to the `keys/` folder at the top of a RIOT checkout.

If the chosen key doesn't exist, it will be generated automatically.
That step can be done manually using the `suit/genkey` target.


## Perform an update

Copy the following files MANIFEST, SLOT0.BIN, SLOT1.BIN into the FIRMWARE directory of the SDCard.

Plug the SDCard into the module.

Push the user button (blue one on the Nucleo board) or type the update command into the shell.


### Node

For the suit_update to work there are important modules that aren't normally built
in a RIOT application:

* riotboot
    * riotboot_flashwrite
* suit
    * suit_transport_fatfs

#### riotboot

To be able to receive updates, the firmware on the device needs a bootloader
that can decide from witch of the firmware images (new one and olds ones) to boot.

For suit updates you need at least two slots in the current conception on riotboot.
The flash memory will be divided in the following way:

```
|------------------------------- FLASH ------------------------------------------------------------|
|-RIOTBOOT_LEN-|------ RIOTBOOT_SLOT_SIZE (slot 0) ------|------ RIOTBOOT_SLOT_SIZE (slot 1) ------|
               |----- RIOTBOOT_HDR_LEN ------|           |----- RIOTBOOT_HDR_LEN ------|
 --------------------------------------------------------------------------------------------------|
|   riotboot   | riotboot_hdr_1 + filler (0) | slot_0_fw | riotboot_hdr_2 + filler (0) | slot_1_fw |
 --------------------------------------------------------------------------------------------------|
```

The riotboot part of the flash will not be changed during suit_updates but
be flashed a first time with at least one slot with suit_capable fw.

    $ BOARD=samr21-xpro make -C examples/suit_update clean flash

When calling make with the `flash` argument it will flash the bootloader
and then to slot0 a copy of the firmware you intend to build.

New images must be of course written to the inactive slot, the device mist be able
to boot from the previous image in case the update had some kind of error, eg:
the image corresponds to the wrong slot.

On boot the bootloader will check the `riotboot_hdr` and boot on the newest
image.

`riotboot_flashwrite` module is needed to be able to write the new firmware to
the inactive slot.

riotboot is not supported by all boards. The default board is `samr21-xpro`,
but any board supporting `riotboot`, `flashpage` and with 256kB of flash should
be able to run the demo.


#### Key Generation

To sign the manifest and for the device to verify the manifest a pair of keys
must be generated. Note that this is done automatically when building an
updatable RIOT image with `riotboot` or `suit/publish` make targets.

This is simply done using the `suit/genkey` make target:

    $ BOARD=nucleo-f411re make -C examples/suit_update_fatfs suit/genkey

You will get this message in the terminal:

    Generated public key: 'a0fc7fe714d0c81edccc50c9e3d9e6f9c72cc68c28990f235ede38e4553b4724'
