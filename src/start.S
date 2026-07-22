
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

  # Zero-initialize the bss section (holding uninitialized globals)
  .Lclear_bss:
  cmp   x0, x1
  b.hs  .Lclear_bss_done
  str   xzr, [x0], #8
  b     .Lclear_bss
  .Lclear_bss_done:

  # Link the VBAR (Vector Base Address Register)
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  # Enter the kernel
  bl    kern_main

  # If we ever return from kern_main here, we fall into the park method below

park:
  wfe
  b park
