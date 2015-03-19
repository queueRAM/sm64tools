# sm64tools
Collection of tools for manipulating the Super Mario 64 ROM

## sm64extend
Super Mario 64 ROM Extender
 - accepts Z64 (BE) or V64 (byte-swapped) ROMs as input
 - works with both US and Japanese ROMs (other tools may not)
 - decompresses all MIO0 blocks from ROM to extended area
 - configurable extended ROM size (default 64 MB)
 - configurable padding between MIO0 blocks (default 32 KB)
 - configurable MIO0 block alignment (default 1 byte)
 - changes all 0x18 commands to 0x17
 - creates MIO0 headers for all 0x1A commands
 - optionally fills old MIO0 blocks with 0x01
 - optionally dump compressed and uncompressed MIO0 data to files
 - updates assembly reference to MIO0 blocks
 - recalculates ROM header checksums

### Usage
```
sm64extend [-s SIZE] [-p PADDING] [-a ALIGNMENT] [-d] [-f] [-v] FILE [OUT_FILE]
```
Options:
 - <code>-s size</code> Size of the extended ROM in MB (default: 64).
 - <code>-p padding</code> Padding to insert between MIO0 blocks in KB (default = 32).
 - <code>-a alignment</code> Byte boundary to align MIO0 blocks (default = 1).
 - <code>-d</code> Dump MIO0 blocks to files in mio0 directory.
 - <code>-f</code> Fill old MIO0 blocks with 0x01.
 - <code>-v</code> Verbose output.

Output file: If unspecified, it is constructed by replacing input file extension with .ext.z64
              
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
```
sm64compress [-a ALIGNMENT] [-c] [-d] [-v] FILE [OUT_FILE]
```
Options:
 - <code>-a alignment</code> Byte boundary to align MIO0 blocks (default = 16).
 - <code>-c</code> Compress all blocks using MIO0.
 - <code>-d</code> Dump MIO0 blocks to files in mio0 directory.
 - <code>-v</code> Verbose output.

Output file: If unspecified, it is constructed by replacing input file extension with .out.z64
