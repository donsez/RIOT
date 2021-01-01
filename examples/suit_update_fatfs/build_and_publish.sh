#!/bin/bash

APP_NAME=suit_update_fatfs
RIOTBASE=~/github/donsez/RIOT
BOARD=nucleo-f411re
BINDIR_APP=bin/$BOARD
KEY_FOR_SIGNATURE=$RIOTBASE/keys/default.pem

# Build and flash the current version	
EPOCH=$(date +%s)
APP_VER=$EPOCH

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST flash

# Build a "new" version	
EPOCH=$(date +%s)
APP_VER=$EPOCH

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST \
	suit/publish

# Rename the slot files for FAT32 name (8 char max + ext)  
cp $BINDIR_APP/$APP_NAME-slot0.$APP_VER.riot.bin $BINDIR_APP/SLOT0.BIN
cp $BINDIR_APP/$APP_NAME-slot1.$APP_VER.riot.bin $BINDIR_APP/SLOT1.BIN

# Generate the manifest  
$RIOTBASE/dist/tools/suit/gen_manifest.py  \
	--urlroot fatfs://FIRMWARE \
	--seqnr $APP_VER \
	--uuid-vendor "riot-os.org" \
	--uuid-class $BOARD \
	-o $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin.tmp \
	$BINDIR_APP/SLOT0.BIN:0x4000 $BINDIR_APP/SLOT1.BIN:262144
	
$RIOTBASE/dist/tools/suit/suit-manifest-generator/bin/suit-tool create -f suit \
	-i $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin.tmp \
	-o $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin

rm $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin.tmp

# Sign the manifest  
$RIOTBASE/dist/tools/suit/suit-manifest-generator/bin/suit-tool sign \
	 -k $KEY_FOR_SIGNATURE \
	 -m $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin \
	 -o $BINDIR_APP/$APP_NAME-riot.suit_signed.$APP_VER.bin

# Copy the manifest and the 2 slot files into the SDCard
cp $BINDIR_APP/$APP_NAME-riot.suit_signed.$APP_VER.bin $BINDIR_APP/MANIFEST

cp $BINDIR_APP/MANIFEST $BINDIR_APP/SLOT*.BIN /Volumes/RIOTSUIT/FIRMWARE/

echo "app_name $APP_NAME" > /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "uuid-vendor riot-os.org" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "seqnr $APP_VER" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "uuid-class $BOARD" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION

open /Volumes/RIOTSUIT/FIRMWARE/
