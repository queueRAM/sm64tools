.n64
.create "example1.bin", 0x0

//-------------------------------------------------
// N64 header
//-------------------------------------------------
.word  0x80371240 // PI BSD Domain 1 register
.word  0x0000000F // clock rate setting
.word  EntryPoint // entry point
.word  0x00001444 // release
.word  0x635A2BFF // checksum1
.word  0x8B022326 // checksum2
.word  0x00000000 // unknown
.word  0x00000000 // unknown"
.ascii "TEST BINARY ROM     " // ROM name: 20 bytes
.word  0x00000000 // unknown
.word  0x0000004E // cartridge
.ascii "TS"       // cartridge ID
.ascii "E"        // country
.byte  0x00       // version

//-------------------------------------------------
// begin asm section
//-------------------------------------------------
.definelabel stack_start, 0x80400000

.headersize 0x80100000 - orga()

// .text
EntryPoint:
   lw  sp, stack_start
   jal main
    nop
   j   EntryPoint
    nop

main:
   addiu sp, sp, -0x18
   sw    ra, 0x14(sp)
   la    t1, data0
   la    t2, data1
   beq   t1, t2, @@done
    nop
@@loop:
   lw    t3, 0(t1)
   addiu t1, t1, 4
   bne   t1, t2, @@loop
    nop
@@done:
   lw    ra, 0x14(sp)
   jr    ra
   addiu sp, sp, 0x18

// .data
.align 0x10
data0:
  .dw 0x00000000, 0x11111111, 0x22222222, 0x33333333
data1:
  .dw 0x44444444, 0x55555555, 0x66666666, 0x77777777
data_ptrs:
  .dw data0, data1
texture_ptrs:
  .dw texture_rgba, texture_ia
file_ptrs:
  .dw block_mio0_start, block_mio0_end, block_gz_start, block_gz_end

// put textures in its own section
.align 0x10
.headersize 0x04000000 - orga()

texture_rgba:
.incbin "bin/texture_rgba16.bin"
texture_ia:
.incbin "bin/texture_ia16.bin"

.dw 0xcafeface

// binary blocks use absolute
.headersize 0x10
.align 16

//-------------------------------------------------
// MIO0 block
//-------------------------------------------------
block_mio0_start:
.incbin "bin/texture_block.mio0"
block_mio0_end:

.dw 0xdeadbeef

//-------------------------------------------------
// gzip block
//-------------------------------------------------
.align 16
block_gz_start:
.incbin "bin/texture_block.bin.gz"
block_gz_end:

.close
