# sm64tools
Collection of tools for manipulating the Super Mario 64 ROM

## n64split
N64 ROM Splitter and Build System
 - splits ROM into assets: asm, textures, models, levels, behavior data
 - generates build files to rebuild the ROM
 - intelligent recursive disassembler
 - generic config file system to support multiple games

### Usage
```console
n64split [-c CONFIG] [-k] [-m] [-o OUTPUT_DIR] [-s SCALE] [-t] [-v] [-V] ROM
```
Options:
 - <code>-c CONFIG</code> ROM configuration file (default: auto-detect)
 - <code>-k</code> keep going as much as possible after error
 - <code>-m</code> merge related instructions in to pseudoinstructions
 - <code>-o OUTPUT_DIR</code> output directory (default: {CONFIG.basename}.split)
 - <code>-s SCALE</code> amount to scale models by (default: 1024.0)
 - <code>-t</code> generate large texture for MIO0 blocks
 - <code>-v</code> verbose output
 - <code>-V</code> print version information

## sm64extend
Super Mario 64 ROM Extender
 - accepts Z64 (BE), V64 (byte-swapped), or N64 (little-endian) ROMs as input
 - works with US, European, Japanese, and Shindou ROMs
 - decompresses all MIO0 blocks from ROM to extended area
 - configurable extended ROM size (default 64 MB)
 - configurable padding between MIO0 blocks (default 32 KB)
 - configurable MIO0 block alignment (default 1 byte)
 - changes all 0x18 level commands to 0x17
 - creates MIO0 headers for all 0x1A level commands
 - optionally fills old MIO0 blocks with 0x01
 - optionally dump compressed and uncompressed MIO0 data to files
 - updates assembly reference to MIO0 blocks
 - recalculates ROM header checksums

### Usage
```console
sm64extend [-a ALIGNMENT] [-p PADDING] [-s SIZE] [-d] [-f] [-v] FILE [OUT_FILE]
```
Options:
 - <code>-a ALIGNMENT</code> Byte boundary to align MIO0 blocks (default = 1).
 - <code>-p PADDING</code> Padding to insert between MIO0 blocks in KB (default = 32).
 - <code>-s SIZE</code> Size of the extended ROM in MB (default: 64).
 - <code>-d</code> Dump MIO0 blocks to files in mio0 directory.
 - <code>-f</code> Fill old MIO0 blocks with 0x01.
 - <code>-v</code> verbose output.

Output file: If unspecified, it is constructed by replacing input file extension with .ext.z64
              
### Examples
64 MB extended ROM that is bit compatible with with generated from the M64ROMExtender1.3b, after extending to 64 MB
```console
sm64extend sm64.z64
```
               
24 MB extended ROM that is bit compatible with the ROM generated from the M64ROMExtender1.3b
```console
sm64extend -s 24 sm64.z64
```
                
Enable verbose messages and specify output filename:
```console
sm64extend -v sm64.z64 sm64_output.ext.z64
```
                 
Pad 64 KB between blocks, align blocks to 16-byte boundaries, fill old MIO0 blocks with 0x01:
```console
sm64extend -p 64 -a 16 -f sm64.z64
```

## sm64compress
Experimental Super Mario 64 ROM alignment and compression tool
 - packs all MIO0 blocks together, reducing unused space
 - optionally compresses MIO0 blocks (and converts 0x17 commands to 0x18)
 - configurable MIO0 block alignment (default 16 byte)
 - reduces output ROM size to 4 MB boundary
 - updates assembly reference to MIO0 blocks
 - recalculates ROM header checksums

### Usage
```console
sm64compress [-a ALIGNMENT] [-c] [-d] [-v] FILE [OUT_FILE]
```
Options:
 - <code>-a alignment</code> Byte boundary to align MIO0 blocks (default = 16).
 - <code>-c</code> compress all blocks using MIO0.
 - <code>-d</code> dump MIO0 blocks to files in mio0 directory.
 - <code>-v</code> verbose output.

Output file: If unspecified, it is constructed by replacing input file extension with .out.z64

## Other Tools
There are many other smaller tools included to help with SM64 hacking.  They are:
 - f3d: tool to decode Fast3D display lists
 - mio0: standalone MIO0 compressor/decompressor
 - n64cksum: standalone N64 checksum generator.  can either do in place or output to a new file
 - n64graphics: converts graphics data from PNG files into RGBA or IA N64 graphics data
 - mipsdisasm: standalone recursive MIPS disassembler
 - sm64geo: standalone SM64 geometry layout decoder

## License

MIT License. Copyright 2015 queueRAM.
