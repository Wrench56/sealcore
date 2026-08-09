OUTPUT_FORMAT("binary")
ENTRY(main)
SECTIONS
{
  . = SEALCORE_ENTRY_OFFSET;

  .text : {
    code = .;
    *(.text.entry)
    *(.text .text.*)
    *(.rodata .rodata.* .rdata .rdata.*)
    . = ALIGN(16);
  }

  .data : {
    data = .;
    *(.data .data.*)
    . = ALIGN(16);
  }

  .bss : {
    bss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(16);
  }

  end = .;

  /DISCARD/ : {
    *(.pdata)
    *(.xdata)
    *(.comment)
    *(.note.*)
    *(.rel.*)
    *(.rela.*)
  }
}
