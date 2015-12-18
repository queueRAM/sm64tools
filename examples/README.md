# Example modifications

This directory contains examples for replacing textures, patching assembly 
source, and updating binaries. The steps for applying the examples to a split 
ROM are given below. Most examples include a batch file to automate the process.

## Hello World
Print Hello World to the screen by hooking into a behavior and overriding an 
unused function. Kills a butterfly.

1. patch -p0 < sm64.hello\_world.patch

Thanks to Jedi for implementing this example and Kaze for his ASM Tutorial:
http://www.smwcentral.net/?p=viewthread&t=68900

## Texture swap

### Castle grounds
Adds a little color to the castle exterior texture in castle grounds (RGBA).

1. copy texture\_swap/castle\_grounds\_textures.0x01000.png to sm64.split/textures/

### Peach's signature
Replace Peach's signature (IA8) with someone else's.

1. copy texture\_swap/castle\_grounds\_segment7.0x0EAE8.ia8.png to sm64.split/textures/

### Transition
Swap star transition with mushroom.

1. copy texture\_swap/font\_graphics.0x122B8.ia8.png to sm64.split/textures/

### Skybox
Replace water skybox with night sky from Lylat System.

1. copy texture\_swap/water\_skybox.0x00000.skybox.png to sm64.split/textures/

## HUD toggle
Source patch for R-button toggle show HUD ASM

1. patch -p0 < sm64.hudtoggle.patch

Thanks to Skelux and Kaze for each of their HUD toggling implementations:
https://sites.google.com/site/supermario64starroad/home/sm64-documents/skelux
http://smwc.me/1208284

## Coin colors
Coin vertex colors and texture replacement.

1. copy the png files from coin\_colors/ to sm64.split/textures/
2. Apply coin\_colors.ips IPS patch to sm64.split/bin/doors\_trees\_coins.bin

Thanks to cpuHacka101 for details on this
http://www.smwcentral.net/?p=viewthread&t=58544

## Behavior
Source patch for modifying the sign posts behavior to rotate about the Y-axis

1. patch -p0 < sm64.rotating\_sign.patch

## Skip Screens
Skip title, Mario, menu screens at startup or Peach and Lakitu intros.

1. patch -p0 < skip\_mario.patch
2. patch -p0 < skip\_title.patch
3. patch -p0 < skip\_menu.patch
4. patch -p0 < skip\_peach.patch
5. patch -p0 < skip\_lakitu.patch
