
.section .text

.global vector_table

halt:
  b .

# The entry saves/dumps the caller-saved registers to the stack, so that the
# exception handler (implemented in C++) can read the contents
exception_entry:
  sub   sp, sp, #192
  stp   x0, x1, [sp, #0]
  stp   x2, x3, [sp, #16]
  stp   x4, x5, [sp, #32]
  stp   x6, x7, [sp, #48]
  stp   x8, x9, [sp, #64]
  stp   x10, x11, [sp, #80]
  stp   x12, x13, [sp, #96]
  stp   x14, x15, [sp, #112]
  stp   x16, x17, [sp, #128]
  stp   x18, x29, [sp, #144]
  stp   x30, xzr, [sp, #160]

  mov   x0, sp
  bl    exception_handler
  cbnz  x0, .Lpanic

  ldp   x0, x1, [sp, #0]
  ldp   x2, x3, [sp, #16]
  ldp   x4, x5, [sp, #32]
  ldp   x6, x7, [sp, #48]
  ldp   x8, x9, [sp, #64]
  ldp   x10, x11, [sp, #80]
  ldp   x12, x13, [sp, #96]
  ldp   x14, x15, [sp, #112]
  ldp   x16, x17, [sp, #128]
  ldp   x18, x29, [sp, #144]
  ldp   x30, xzr, [sp, #160]
  add   sp, sp, #192
  eret

.Lpanic:
  b halt

irq_entry:
  sub   sp, sp, #192
  stp   x0, x1, [sp, #0]
  stp   x2, x3, [sp, #16]
  stp   x4, x5, [sp, #32]
  stp   x6, x7, [sp, #48]
  stp   x8, x9, [sp, #64]
  stp   x10, x11, [sp, #80]
  stp   x12, x13, [sp, #96]
  stp   x14, x15, [sp, #112]
  stp   x16, x17, [sp, #128]
  stp   x18, x29, [sp, #144]
  stp   x30, xzr, [sp, #160]

  mov   x0, sp
  mrs   x1, ICC_IAR1_EL1
  bl    irq_handler
  msr   ICC_EOIR1_EL1, x0

  ldp   x0, x1, [sp, #0]
  ldp   x2, x3, [sp, #16]
  ldp   x4, x5, [sp, #32]
  ldp   x6, x7, [sp, #48]
  ldp   x8, x9, [sp, #64]
  ldp   x10, x11, [sp, #80]
  ldp   x12, x13, [sp, #96]
  ldp   x14, x15, [sp, #112]
  ldp   x16, x17, [sp, #128]
  ldp   x18, x29, [sp, #144]
  ldp   x30, xzr, [sp, #160]
  add   sp, sp, #192
  eret

.macro vector label
.p2align 7
b   \label
.endm

.p2align 11
vector_table:
  # Order is:
  # 0: Synchronous/Debug (CPU exception occurred due to instruction)
  # 1: IRQ (Interrupt REquest)
  # 2: FIQ (Fast Interrupt Request)
  # 3: SError (CPU exception unrelated to instructions)

  # EL1 keep stack
  vector .
  vector .
  vector .
  vector .

  # ELn own stack (kernel mode)
  vector exception_entry
  vector irq_entry
  vector exception_entry
  vector exception_entry

  # EL(n-1)->ELn (user mode)
  vector .
  vector .
  vector .
  vector .

  # 32-bit EL(n-1)->ELn
  vector .
  vector .
  vector .
  vector .
