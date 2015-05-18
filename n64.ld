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
      * (.mio0);
      * (.rodata);
      * (.data);
      * (.MIPS.abiflags);
      /* default 4MB data (12MB ROM) */
      . = 0x400000;
   }

   /* (0x8016F000, 0x801B99DF, 0x7FF4FB40), // 21F4C0-269E9F  4A9E0 */
   . = 0x8016F000;
   .text0x8016F000 : AT(0x21F4C0) {
      * (.text0x8016F000);
   }

   /* run code from 80245000, but load to 0x000000 */
   . = 0x80245000;
   .text : AT(0x000000) {
      FILL (0xFF)
      * (.header);
      * (.text);
   }

   /* (0x80378800, 0x8038BC8F, 0x80283280)  // 0F5580-108A0F  13490 */
   . = 0x80378800;
   .text0x80378800 : AT(0x0F5580) {
      * (.text0x80378800);
   }

   /*. = 0x800000;*/
}
