#!/bin/bash

RIOTBASE=~/github/bergzand/RIOT
EPOCH=$(date +%s)
APP_VER=$EPOCH
BOARD=nucleo-f411re
BINDIR_APP=bin/$BOARD

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST
	
gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST \
	suit/publish

cp $BINDIR_APP/suit_update_fatfs-slot0.$APP_VER.riot.bin $BINDIR_APP/SLOT0.BIN
cp $BINDIR_APP/suit_update_fatfs-slot1.$APP_VER.riot.bin $BINDIR_APP/SLOT1.BIN

$RIOTBASE/dist/tools/suit/gen_manifest.py  \
	--urlroot fatfs://FIRMWARE \
	--seqnr $APP_VER \
	--uuid-vendor "riot-os.org" \
	--uuid-class $BOARD \
	-o $BINDIR_APP/suit_update_fatfs-riot.suit.$APP_VER.bin.tmp \
	$BINDIR_APP/SLOT0.BIN:0x4000 $BINDIR_APP/SLOT1.BIN:262144
	
$RIOTBASE/dist/tools/suit/suit-manifest-generator/bin/suit-tool create -f suit \
	-i $BINDIR_APP/suit_update_fatfs-riot.suit.$APP_VER.bin.tmp \
	-o $BINDIR_APP/suit_update_fatfs-riot.suit.$APP_VER.bin

rm $BINDIR_APP/suit_update_fatfs-riot.suit.$APP_VER.bin.tmp

$RIOTBASE/dist/tools/suit/suit-manifest-generator/bin/suit-tool sign \
	 -k $RIOTBASE/keys/default.pem \
	 -m $BINDIR_APP/suit_update_fatfs-riot.suit.$APP_VER.bin \
	 -o $BINDIR_APP/suit_update_fatfs-riot.suit_signed.$APP_VER.bin

cp $BINDIR_APP/suit_update_fatfs-riot.suit_signed.$APP_VER.bin $BINDIR_APP/MANIFEST

cp $BINDIR_APP/MANIFEST $BINDIR_APP/SLOT*.BIN /Volumes/RIOTSUIT/FIRMWARE/

open /Volumes/RIOTSUIT/FIRMWARE/
