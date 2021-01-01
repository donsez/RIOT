# name of your application
APPLICATION = suit_update_fatfs

# If no BOARD is found in the environment, use this default:
BOARD ?= nucleo-f411re

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..


# include this for printing IP addresses
USEMODULE += shell
USEMODULE += shell_commands

# Set this to 1 to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 0

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

#
# FatFS SDCard
#

USEMODULE += fatfs_diskio_mtd
USEMODULE += mtd
USEPKG    += fatfs

FATFS_IMAGE_FILE_SIZE_MIB ?= 128

ifeq ($(BOARD),native)
#overwrite default mtd_native-config to use fat image as flash device
CFLAGS += -DMTD_NATIVE_FILENAME=\"./bin/riot_fatfs_disk.img\"
CFLAGS += -DMTD_PAGE_SIZE=512
CFLAGS += -DMTD_SECTOR_SIZE=512
CFLAGS += -DFATFS_IMAGE_FILE_SIZE_MIB=$(FATFS_IMAGE_FILE_SIZE_MIB)
CFLAGS += -DMTD_SECTOR_NUM=\(\(\(FATFS_IMAGE_FILE_SIZE_MIB\)*1024*1024\)/MTD_SECTOR_SIZE\)
else
# for actual hardware use mtd_sdcard as storage device
USEMODULE += mtd_sdcard

# configuration for the sdcard_spi driver
CFLAGS += -DSDCARD_SPI_PARAM_SPI='(SPI_DEV(0))'
CFLAGS += -DSDCARD_SPI_PARAM_CS='(ARDUINO_PIN_4)'
CFLAGS += -DSDCARD_SPI_PARAM_CLK='(ARDUINO_PIN_13)'
CFLAGS += -DSDCARD_SPI_PARAM_MOSI='(ARDUINO_PIN_11)'
CFLAGS += -DSDCARD_SPI_PARAM_MISO='(ARDUINO_PIN_12)'
CFLAGS += -DSDCARD_SPI_PARAM_POWER='(GPIO_UNDEF)'

CFLAGS += -DFATFS_READ_BUFFER_SIZE=512

endif


#
# SUIT update specific stuff
#

CFLAGS += -DLOG_LEVEL=LOG_DEBUG

USEMODULE += suit suit_transport_fatfs suit_storage_flashwrite

# Display a progress bar during firmware download
USEMODULE += progress_bar

# Optional feature to trigger suit update through gpio callback
FEATURES_OPTIONAL += periph_gpio_irq


# Default manifest location when fetched through gpio trigger
CFLAGS += -DSUIT_MANIFEST_RESOURCE=\"$(MANIFEST_URL)\"

# Ensure both slot bin files are always generated and linked to avoid compiling
# during the test. This ensures that "BUILD_IN_DOCKER=1 make test"
# can rely on them being present without having to trigger re-compilation.
BUILD_FILES += $(SLOT_RIOT_ELFS:%.elf=%.bin)

# The test needs the linked slot binaries without header in order to be able to
# create final binaries with specific APP_VER values. The CI RasPi test workers
# don't compile themselves and re-create signed images, thus add the required
# files here so they will be submitted along with the test jobs.
TEST_EXTRA_FILES += $(SLOT_RIOT_ELFS) $(SUIT_SEC) $(SUIT_PUB)
# For now `examples/%/tests" still rely on the test applicaton being reset after
# a terminal is opened to synchronize.
TESTRUNNER_RESET_AFTER_TERM ?= 1

# This can be removed as soon as the Pi-fleet runners support an Openssl version
# with ed25519 support.
TEST_ON_CI_BLACKLIST = all

include $(RIOTBASE)/Makefile.include

.PHONY: host-tools

host-tools:
	$(Q)env -u CC -u CFLAGS $(MAKE) -C $(RIOTTOOLS)

include $(RIOTMAKE)/default-radio-settings.inc.mk
