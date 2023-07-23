OUTPUT_ARCH( "riscv" )
OUTPUT_FORMAT("elf32-littleriscv")
ENTRY( _start )
SECTIONS
{
  /* text: code section */
  . = 0x00000000;

  .text : { *(.text) }

  /* gnu_build_id: readonly build identifier */
  .gnu_build_id : { *(.note.gnu.build-id) }

}
