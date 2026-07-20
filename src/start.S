
.section .text.boot

.global _start

_start:
  # Park all cores that aren't core 0
  mrs   x4, MPIDR_EL1
  and   x4, x4, #0xff
  cbnz  x4, park

  # Initialize stack. We already align it in the linker script to 16 bytes.
  ldr   x0, =__stack_top
  mov   sp, x0

  ldr   x0, =__bss_start
  ldr   x1, =__bss_end

  # Zero initialize the bss (section holding uninitialized globals) section
  .clear_bss:
  cmp   x0, x1
  b.hs  .clear_bss_done
  str   xzr, [x0], #8
  b     .clear_bss
  .clear_bss_done:

  # Setup the exceptions, mask in DAIF and table in VBAR (Vector Base Address Register)
  msr   DAIF, xzr
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  udf   #123

  # Enter the kernel
  bl    kern_main

  # If we ever return from kern_main here, we fall into the park method below

park:
  wfe
  b park
