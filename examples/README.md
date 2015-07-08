# Example modifications

This directory contains examples for replacing textures, patching assembly source, and updating binaries.

## Texture swap
castle grounds texture swap (RGBA)

1. copy 0x01000.png to gen/textures/castle\_grounds\_textures/

## Peach's signature
replace peach's signature (IA8) with my hero's autograph

1. copy signature/0x0EAE8.ia8.png to gen/textures/castle\_grounds\_geo/

## Transition
swap star transition with mushroom

1. copy transition/0x122B8.ia8.png to gen/textures/font\_graphics/

## HUD toggle
Source patch for R-button toggle show HUD ASM

1. patch -p0 < sm64.hudtoggle.patch

Thanks to Skelux's implementation from
https://sites.google.com/site/supermario64starroad/home/sm64-documents/skelux

## Coin colors
coin colors and texture replacement

1. copy 0x05780.ia16.png 0x05F80.ia16.png 0x06780.ia16.png 0x06F80.ia16.png to gen/textures/doors\_trees\_coins/
2. Apply coin\_colors.ips IPS patch to gen/bin/doors\_trees\_coins.bin

Thanks to cpuHacka101 for details on this
http://www.smwcentral.net/?p=viewthread&t=58544

## Skybox
replace water skybox with night sky from Lylat System.

1. copy skybox/0x00000.skybox.png to gen/textures/water\_skybox/

## Behavior
Source patch for modifying the sign posts behavior to rotate about the Y-axis

1. patch -p0 < sm64.rotating\_sign.patch

## Skip Screens
Skip title, Mario, or menu screens at startup

1. patch -p0 < skip\_mario.patch
2. patch -p0 < skip\_title.patch
3. patch -p0 < skip\_menu.patch
