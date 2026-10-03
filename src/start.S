
.section .text.boot

.global _start
.global _secondary_start

#  Entrypoint for the boot CPU (CPU 0)
_start:
  # Stash away the pointer to the FDT, which arrives in x0 from the bootloader
  mov   x19, x0

  # Maybe drop from EL2 to EL1
  bl    drop_el2_to_el1

  # Park all cores that aren't core 0 if they ever get in here. If cores are
  # halted/turned off during startup, they are awoken to the _secondary_start
  # function below, via PSCI CPU_ON or a SpinTable.
  mrs   x0, MPIDR_EL1
  and   x0, x0, #0xff
  cbnz  x0, park

  bl    setup_stack

  # Link the VBAR (Vector Base Address Register)
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  # Zero-initialize the bss section (holding uninitialized globals)
  ldr   x0, =__bss_start
  ldr   x1, =__bss_end
  .Lclear_bss:
  cmp   x0, x1
  b.hs  .Lclear_bss_done
  str   xzr, [x0], #8
  b     .Lclear_bss
  .Lclear_bss_done:
  dsb   sy

  # Call global constructors (.init_array). Must be done after .bss zero initialization
  ldr   x20, =__init_array_start
  ldr   x21, =__init_array_end
  .Linit_array_call:
  cmp   x20, x21
  b.hs  .Linit_array_call_done
  ldr   x0, [x20]
  blr   x0
  add   x20, x20, #8
  b     .Linit_array_call
  .Linit_array_call_done:

  # Restore the stashed FDT pointer and enter the kernel
  mov   x0, x19
  bl    kern_main
  b     park

# Entrypoint for secondary CPUs (CPU >0). Called from PSCI CPU_ON or SpinTable
# enable methods.
_secondary_start:
  bl    drop_el2_to_el1

  mrs   x0, MPIDR_EL1
  and   x0, x0, #0xff
  bl    setup_stack

  # Link the VBAR (Vector Base Address Register)
  ldr   x0, =vector_table
  msr   VBAR_EL1, x0
  isb

  bl    secondary_kern_main
  b     park

# Checks if the CPU is at EL2 and if so drops from EL2 to EL1
drop_el2_to_el1:
  # Check the exception level the kernel boots into
  mrs   x0, CurrentEL
  lsr   x0, x0, #2
  and   x0, x0, #0b11

  # If booted at EL2 drop to EL1
  cmp   x0, #2
  b.ne  .Lreturn

  # Bit [31]: Execution state control for lower Exception levels:
  # The Execution state for EL1 is AArch64. The Execution state for EL0 is
  # determined by the current value of PSTATE.nRW when executing at EL0.
  mov   x0, #1
  lsl   x0, x0, #31
  msr   HCR_EL2, x0

  # Traps to EL2 are more or less common, so we link the VBAR (Vector Base
  # Address Register).
  ldr   x0, =vector_table
  msr   VBAR_EL2, x0
  isb

  mov   x0, xzr
  # Mask out the DAIF bits
  orr   x0, x0, #(0b1111 << 6)
  # Set the M bits (Exception level and selected Stack Pointer) to EL1 with SP_EL1 (EL1h)
  orr   x0, x0, #(1 << 2)
  orr   x0, x0, #(1 << 0)
  msr   SPSR_EL2, x0

  ldr   x0, =.Lreturn
  msr   ELR_EL2, x0
  eret

  .Lreturn:
  ret

# Sets up the stack for a specific CPU core
setup_stack:
  # The cpu id is in x0, now we need to adjust by the stack size, which is 0x10000 (64 KiB)
  ldr   x1, =__stacks_bottom
  ldr   x2, =STACK_SIZE
  madd  x1, x0, x2, x1  /* x1 = __stacks_bottom + cpu_index * STACK_SIZE */
  add   sp, x1, x2
  ret

# Parking space
park:
  wfe
  b park
