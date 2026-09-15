
.section .text

.global vector_table

.macro save_registers
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
.endm

.macro restore_registers
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
.endm

exception_entry:
  save_registers
  mov   x0, sp
  bl    exception_handler
  cbnz  x0, .Lpanic
  restore_registers
  eret

.Lpanic:
  b .

irq_entry:
  save_registers
  mov   x1, sp

  bl    acknowledge_interrupt
  bl    irq_handler
  bl    end_of_interrupt

  restore_registers
  eret

fiq_entry:
  save_registers
  mov   x1, sp

  bl    acknowledge_interrupt
  bl    irq_handler
  bl    end_of_interrupt

  restore_registers
  eret

.macro vector label
.p2align 7
  b \label
.endm

.p2align 11
vector_table:
  # Order is:
  # 0: Synchronous/Debug (CPU exception occurred due to instruction)
  # 1: IRQ (Interrupt Request)
  # 2: FIQ (Fast Interrupt Request)
  # 3: SError (CPU exception unrelated to instructions)

  # ELn keep stack (SP_EL0 was used)
  vector .
  vector .
  vector .
  vector .

  # ELn own stack (kernel mode, SP_ELx (x>0) was used)
  vector exception_entry
  vector irq_entry
  vector fiq_entry
  vector exception_entry

  # 64-bit EL(n-1)->ELn
  vector .
  vector .
  vector .
  vector .

  # 32-bit EL(n-1)->ELn
  vector .
  vector .
  vector .
  vector .
