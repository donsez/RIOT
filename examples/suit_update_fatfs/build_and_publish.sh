#!/bin/bash

APP_NAME=suit_update_fatfs

RIOTBASE=../..
BOARD=nucleo-f411re
BINDIR_APP=bin/$BOARD
KEY_FOR_SIGNATURE=$RIOTBASE/keys/default.pem

echo BOARD=$BOARD
echo KEY=$BOARD


EPOCH=$(date +%s)
APP_VER=$EPOCH

echo
echo ---------------------------------------------------------
echo  'Build the current version ' $APP_VER for $BOARD
echo ---------------------------------------------------------

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST

echo
echo ---------------------------------------------------------
echo  'Flash the current version ' $APP_VER for $BOARD
echo ---------------------------------------------------------

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST flash-only


EPOCH=$(date +%s)
APP_VER=$EPOCH

echo
echo ---------------------------------------------------------
echo 'Build a "new" version' $APP_VER for $BOARD
echo ---------------------------------------------------------

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST

gmake BOARD=$BOARD \
	SUIT_COAP_ROOT=fatfs://FIRMWARE \
	APP_VER=$APP_VER \
	MANIFEST_URL=fatfs://FIRMWARE/MANIFEST \
	suit/publish

echo
echo ---------------------------------------------------------
echo 'Rename the slot files for FAT32 name (8 char max + ext)'
echo ---------------------------------------------------------
cp $BINDIR_APP/$APP_NAME-slot0.$APP_VER.riot.bin $BINDIR_APP/SLOT0.BIN
cp $BINDIR_APP/$APP_NAME-slot1.$APP_VER.riot.bin $BINDIR_APP/SLOT1.BIN

echo
echo ---------------------------------------------------------
echo 'Generate the manifest'
echo ---------------------------------------------------------
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

echo
echo ---------------------------------------------------------
echo 'Sign the manifest'  
echo ---------------------------------------------------------
$RIOTBASE/dist/tools/suit/suit-manifest-generator/bin/suit-tool sign \
	 -k $KEY_FOR_SIGNATURE \
	 -m $BINDIR_APP/$APP_NAME-riot.suit.$APP_VER.bin \
	 -o $BINDIR_APP/$APP_NAME-riot.suit_signed.$APP_VER.bin

echo
echo ---------------------------------------------------------
echo 'Compute binary diff between slot0 and slot1' 
echo ---------------------------------------------------------
#cmp -l $BINDIR_APP/SLOT0.BIN $BINDIR_APP/SLOT1.BIN \
#	| gawk '{printf "%08X %02X %02X\n", $1, strtonum(0$2), strtonum(0$3)}' >  $BINDIR_APP/SLOT1.DIF
cmp -l $BINDIR_APP/SLOT0.BIN $BINDIR_APP/SLOT1.BIN \
	| gawk '{printf "%06X%02X\n", $1, strtonum(0$3)}' | xxd -r -p > $BINDIR_APP/SLOT1.DIF


echo
echo ---------------------------------------------------------
echo 'Copy the manifest and the 2 slot files into the SDCard'
echo ---------------------------------------------------------
cp $BINDIR_APP/$APP_NAME-riot.suit_signed.$APP_VER.bin $BINDIR_APP/MANIFEST

cp $BINDIR_APP/MANIFEST $BINDIR_APP/SLOT*.BIN $BINDIR_APP/SLOT1.DIF /Volumes/RIOTSUIT/FIRMWARE/

echo
echo ---------------------------------------------------------
echo 'Generate and copy the VERSION file into the SDCard'
echo ---------------------------------------------------------

echo "app_name $APP_NAME" > /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "uuid-vendor riot-os.org" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "seqnr $APP_VER" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION
echo "uuid-class $BOARD" >> /Volumes/RIOTSUIT/FIRMWARE/VERSION
cat /Volumes/RIOTSUIT/FIRMWARE/VERSION

open /Volumes/RIOTSUIT/FIRMWARE/
