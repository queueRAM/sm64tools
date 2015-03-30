OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)

/*
MEMORY
{
   rom : ORIGIN = 0x80245000, LENGTH = 0xC00000
}
*/

SECTIONS
{
   /* load MIO0 and level data at 0x800000 */
   . = 0x800000;
   .rodata : {
      FILL (0x01) /* fill unused with 0x01 */
      * (mio0);
      * (.rodata);
      /* default 4MB data (12MB ROM) */
      . = 0x400000;
   }
   /* run code from 80245000, but load to 0x000000 */
   . = 0x80245000;
   .text : AT(0x000000) {
      FILL (0xFF)
      * (header);
      * (boot);
      * (.text);
      * (.data);
      * (.MIPS.abiflags);
   }
   /*. = 0x800000;*/
}
