## sm64extend: Super Mario 64 ROM Extender
 - accepts Z64 (BE), V64 (byte-swapped), or N64 (little-endian) ROMs as input
 - works with US, European, Japanese, Shindou, and iQue ROMs
 - decompresses all MIO0 blocks from ROM to extended area
 - configurable extended ROM size (default 64 MB)
 - configurable padding between MIO0 blocks (default 32 KB)
 - configurable MIO0 block alignment (default 1 byte)
 - changes all 0x18 level commands to 0x17
 - creates MIO0 headers for all 0x1A level commands
 - optionally dump compressed and uncompressed MIO0 data to files
 - optionally fills old MIO0 blocks with 0x01
 - updates assembly reference to MIO0 blocks
 - recalculates ROM header checksums

### Usage
```
sm64extend [-a ALIGNMENT] [-p PADDING] [-s SIZE] [-d] [-f] [-v] FILE [OUT_FILE]
```
Options:
 -a ALIGNMENT Byte boundary to align MIO0 blocks (default = 1).
 -p PADDING   Padding to insert between MIO0 blocks in KB (default = 32).
 -s SIZE      Size of the extended ROM in MB (default: 64).
 -d           dump MIO0 blocks to files in 'mio0files' directory
 -f           Fill old MIO0 blocks with 0x01.
 -v           Verbose output.

File arguments:
 FILE        input ROM file
 OUT_FILE    output ROM file (default: replaces FILE extension with .ext.z64)

### Examples
64 MB extended ROM that is bit compatible with with generated from the M64ROMExtender1.3b, after extending to 64 MB
```
sm64extend sm64.z64
```
               
24 MB extended ROM that is bit compatible with the ROM generated from the M64ROMExtender1.3b
```
sm64extend -s 24 sm64.z64
```
                
Enable verbose messages and specify output filename:
```
sm64extend -v sm64.z64 sm64_output.ext.z64
```
                 
Pad 64 KB between blocks, align blocks to 16-byte boundaries, fill old MIO0 blocks with 0x01:
```
sm64extend -p 64 -a 16 -f sm64.z64 sm64_output.ext.z64
```
### Changelog:
v0.3.2: add support for SM64 iQue ROM
v0.3.1: add little-endian ROM support and error checking
 - add support for .n64 little-endian ROMs
 - add further validation on SM64 ROM version
 - restrict range of output size to between 16 and 64MB
v0.3: add support for (E) and Shindou ROMs
 - increase detection of level script and asm references
 - detect optimized ASM references
 - statically link win32 build to drop MSVCR120.dll dependency
v0.2.1: add dump option and ROM validity checks
 - add dump (-d) command line option
 - add more ROM validity checks on input file
v0.2: more options, error checking, and allow extending SM64 (J)
 - add fill (-f) command line option
 - add more error checking up front and while extending
 - dynamically find ASM references to MIO0 blocks to support SM64 (J) ROM
v0.1: Initial release
 - supports extending SM64 (U) ROM
