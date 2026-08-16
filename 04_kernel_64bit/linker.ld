ENTRY(_start)
 
 SECTIONS
 {
     /* --- LOW MEMORY TSS SECTION FIRST --- */
     . = 0x5000;
 
     .tss (NOLOAD) : {
         *(.data.tss)
     }
 
     /* --- NOW SWITCH TO HIGHER-HALF KERNEL BASE --- */
     . = 0xFFFFFFFF80100000;
 
     .text : {
         *(.text*)
     }
 
     .rodata : {
         *(.rodata*)
     }
 
     .data : {
         *(.data*)
     }
 
     .bss : {
         *(.bss*)
         *(COMMON)
         . = ALIGN(4096);
         . += 8192;
         . = ALIGN(4096);
     }
 }

