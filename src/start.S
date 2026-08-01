
.section .text.boot

.global _start
.global _secondary_start

_start:
  # Stash away the pointer to the FDT, which arrives in x0 from the bootloader
  mov   x18, x0

  # Park all cores that aren't core 0 if they ever get in here. If cores are
  # halted/turned off during startup, they are awoken to the _secondary_start
  # function below, via PSCI CPU_ON.
  mrs   x0, MPIDR_EL1
  and   x0, x0, #0xff
  cbnz  x0, park

  bl    setup_stack

  # Zero-initialize the bss section (holding uninitialized globals)
  ldr   x0, =__bss_start
  ldr   x1, =__bss_end
  .Lclear_bss:
  cmp   x0, x1
  b.hs  .Lclear_bss_done
  str   xzr, [x0], #8
  b     .Lclear_bss
  .Lclear_bss_done:
  dsb sy

  # Link the VBAR (Vector Base Address Register)
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  # Restore the stashed FDT pointer and enter the kernel
  mov   x0, x18
  bl    kern_main
  b     park

/* This method is entered via a call to PSCI CPU_ON, where we pass the cpu/core
   id in x0 */
_secondary_start:
  mov   x18, x0
  bl    setup_stack

  # Link the VBAR (Vector Base Address Register)
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  mov   x0, x18
  bl    secondary_main
  b     park

setup_stack:
  # The cpu id is in x0, now we need to adjust by the stack size, which is 0x10000 (64 KiB)
  ldr   x1, =__stacks_bottom
  ldr   x2, =STACK_SIZE
  madd  x1, x0, x2, x1  /* x1 = __stacks_bottom + cpu_index * STACK_SIZE */
  add   sp, x1, x2
  ret

park:
  wfe
  b park
