# sm64tools
Collection of tools for manipulating the Super Mario 64 ROM

## sm64extend
Super Mario 64 ROM Extender
 - accepts Z64 (BE) or V64 (byte-swapped) ROMs as input
 - extends ROM to 64 MB (configurable) and fills with 0x01
 - inserts 32 KB padding between MIO0 blocks (configurable)
 - configurable MIO0 block alignment
 - decompresses all MIO0 blocks from ROM to extended area
 - changes all 0x18 commands to 0x17
 - creates MIO0 headers for all 0x1A commands
 - updates assembly reference to first MIO0 block
 - recalculates ROM header checksums

### Usage
Usage:
```
sm64extend [options] <input file> [output file]
```
Options:
 - <code>-s size</code> The desired size of the extended ROM in MB (default = 64).
 - <code>-p padding</code> Padding to insert between MIO0 files in KB (default = 32).
 - <code>-a alignment</code> Byte boundary to align MIO0 files (default = 1).
 - <code>-v</code>Verbose output.

Output file: If unspecified, it is constructed by replacing file extension of input file with .ext.z64
              
### Examples
64 MB extended ROM that is byte compatible with with generated from the M64ROMExtender1.3b, after extending to 64 MB
```
sm64extend sm64.z64
```
               
24 MB extended ROM that is byte compatible with the ROM generated from the M64ROMExtender1.3b
```
sm64extend -s 24 sm64.z64
```
                
Enable verbose messages and specify output filename:
```
sm64extend -v sm64.z64 sm64_output.ext.z64
```
                 
Pad 64 KB between blocks and align blocks to 16-byte boundaries:
```
sm64extend -p 64 -a 16 sm64.z64 sm64_output.ext.z64
```
