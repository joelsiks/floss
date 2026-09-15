#include "interrupts/gicv2.h"

#include <cstring>

#include "DeviceTree.h"
#include "kstdio.h"
#include "util/assert.h"

// GICv2 has a single Distributor (GICD) shared by all Processing Elements
// (CPUs/cores), and a per-PE CPU Interface (GICC). Both are accessed through
// memory-mapped registers. Unlike GICv3 there are no Redistributors and no
// system-register access: acknowledge/end-of-interrupt happen over MMIO.
//
// Some abbreviations:
// SGI - Software Generated Interrupt (CPU-private, banked, INTIDs 0-15)
// PPI - Private Peripheral Interrupt (CPU-private, banked, INTIDs 16-31)
// SPI - Shared Peripheral Interrupt (distributed by the GICD, INTIDs 32-1019)
//
// SGI and PPI (INTIDs 0-31) are banked per core, so the GICD registers that
// cover them (IGROUPR0, ISENABLER0, IPRIORITYR<0..31>, ...) are per-CPU.
// SPI (INTIDs 32+) are shared and configured once in the GICD.

static uintptr_t GICD_BASE = 0;

// Memory Mapped Device Registers for the Distributor
static const uintptr_t GICD_CTLR = 0x0000;
static const uintptr_t GICD_TYPER = 0x0004;
static const uintptr_t GICD_IGROUPR = 0x80;
static const uintptr_t GICD_ISENABLER = 0x100;
static const uintptr_t GICD_ICENABLER = 0x180;

static const uintptr_t GICD_IPRIORITY_BASE = 0x400;
static const uintptr_t GICD_ITARGETSR_BASE = 0x800;

static volatile uint32_t* distributor(uintptr_t offset) {
  return reinterpret_cast<volatile uint32_t*>(GICD_BASE + offset);
}

static uintptr_t GICC_BASE = 0;

// Memory Mapped Device Registers for the CPU Interface
static const uintptr_t GICC_CTLR = 0x0000;
static const uintptr_t GICC_PMR = 0x0004;
static const uintptr_t GICC_BPR = 0x0008;
static const uintptr_t GICC_IAR = 0x000C;
static const uintptr_t GICC_EOIR = 0x0010;

static volatile uint32_t* cpu_interface(uintptr_t offset) {
  return reinterpret_cast<volatile uint32_t*>(GICC_BASE + offset);
}

// GICD_CTLR bits. In a single-security-state configuration (the common case
// for non-secure bare-metal, e.g. QEMU's arm,gic-400) all interrupts live in
// Group 1 and the Group 1 enable bit is used.
static const uint32_t GICD_CTLR_EnableGrp0 = 0b01;
static const uint32_t GICD_CTLR_EnableGrp1 = 0b10;

// GICC_CTLR bits
static const uint32_t GICC_CTLR_Enable = 0b01;
static const uint32_t GICC_CTLR_EnableGrp1 = 0b10;
static const uint32_t GICC_CTLR_AckCtl = 0b100;

// GICD_TYPER: ITLinesNumber (bits[4:0]) gives the number of SPI register
// blocks; the total number of supported INTIDs is 32 * (ITLinesNumber + 1).
static const uint32_t GICD_TYPER_ITLinesNumber_Mask = 0x1F;

static uint32_t NumInterrupts = 0;

void GIC::DriverV2::initialize_gic_distributor() {
  // Disable the Distributor while we configure it.
  *distributor(GICD_CTLR) = 0;

  // Ensure write to GICD_CTLR has completed before continuing
  asm volatile("dsb sy" ::: "memory");

  // Discover the number of supported interrupt IDs.
  uint32_t typer = *distributor(GICD_TYPER);
  uint32_t it_lines = typer & GICD_TYPER_ITLinesNumber_Mask;
  NumInterrupts = 32 * (it_lines + 1);

  // Place all interrupts in Group 1 and disable them, so that nothing fires
  // before being explicitly configured via initialize_interrupt().
  for (uint32_t n = 0; n < (NumInterrupts / 32); n++) {
    const uintptr_t off = n * 4;
    *distributor(GICD_IGROUPR + off) = 0xFFFFFFFFu;   // Group 1
    *distributor(GICD_ICENABLER + off) = 0xFFFFFFFFu; // disabled
  }

  asm volatile("dsb sy" ::: "memory");

  // Enable the Distributor for Group 1.
  *distributor(GICD_CTLR) = GICD_CTLR_EnableGrp1;

  // Ensure write to GICD_CTLR has completed before continuing
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::enable_cpu_interface() {
  // GICC_CTLR controls the CPU Interface. Enable the interface itself; the
  // Group 1 forwarding is enabled separately by enable_cpu_interrupts().
  *cpu_interface(GICC_CTLR) = GICC_CTLR_Enable;
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::enable_cpu_interrupts() {
  // Enable forwarding of Group 1 interrupts to this PE. Read-Modify-Write so
  // the Enable bit set above is preserved.
  uint32_t ctlr = *cpu_interface(GICC_CTLR);
  ctlr |= GICC_CTLR_EnableGrp1 | GICC_CTLR_AckCtl;
  *cpu_interface(GICC_CTLR) = ctlr;
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::set_cpu_priority_mask(uint64_t priority) {
  // GICC_PMR is the priority mask: only interrupts with a higher priority
  // (numerically lower) than this value are signaled to the PE.
  *cpu_interface(GICC_PMR) = static_cast<uint32_t>(priority);
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::set_interrupt_priority(int id, InterruptPriority priority) {
  // GICD_IPRIORITYR<n> is byte-addressable: one byte per interrupt id, for
  // both the banked SGI/PPI range (0-31) and the shared SPI range (32+).
  volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(GICD_BASE + GICD_IPRIORITY_BASE + id);
  *p = static_cast<uint8_t>(priority);

  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::set_interrupt_group(int id) {
  // Place the interrupt in Group 1. GICD_IGROUPR<n> is banked for INTIDs 0-31
  // (per-CPU) and shared for INTIDs 32+, but in both cases the access is the
  // same Read-Modify-Write against the GICD frame.
  const uint32_t reg_index = id / 32;
  const uint32_t reg_offset = reg_index * 4;
  const uint32_t shift = id % 32;

  // Read-Modify-Write
  uint32_t igrouprn = *distributor(GICD_IGROUPR + reg_offset);
  igrouprn |= (1u << shift);
  *distributor(GICD_IGROUPR + reg_offset) = igrouprn;

  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::enable_interrupt(int id) {
  // Both GICD_ISENABLER<n> is "write-1-to-set", so no masking is required to
  // not affect other bits. ICENABLER has the opposite effect of
  // "write-1-to-clear".
  const uint32_t reg_index = id / 32;
  const uint32_t reg_offset = reg_index * 4;
  const uint32_t shift = id % 32;

  *distributor(GICD_ISENABLER + reg_offset) = (1u << shift);
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::disable_interrupt(int id) {
  // GICD_ICENABLER<n> is "write-1-to-clear", so no masking is required to
  // not affect other bits. ISENABLER has the opposite effect of
  // "write-1-to-set".
  const uint32_t reg_index = id / 32;
  const uint32_t reg_offset = reg_index * 4;
  const uint32_t shift = id % 32;

  *distributor(GICD_ICENABLER + reg_offset) = (1u << shift);
  asm volatile("dsb sy" ::: "memory");
}

void GIC::DriverV2::set_interrupt_routing(int id) {
  // GICD_ITARGETSR<n> is byte-addressable and holds a CPU target bitmap
  // (bit i set => forward to PE i). For SGI/PPI (id < 32) the field is
  // read-only and always targets the local CPU, so only configure SPIs.
  if (id > 31) {
    // Target CPU 0 (the boot PE). A more complete driver would compute the
    // affinity and route appropriately.
    const uint8_t target = 0b00000001;
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(GICD_BASE + GICD_ITARGETSR_BASE + id);
    *p = target;

    asm volatile("dsb sy" ::: "memory");
  } else {
    // Banked SGI/PPI: target is fixed to the local CPU, nothing to do.
  }
}

void GIC::DriverV2::set_gicd_base(uintptr_t base) {
  GICD_BASE = base;
}

void GIC::DriverV2::set_gicc_base(uintptr_t base) {
  GICC_BASE = base;
}

// Initialization sequence:
//   GICD (Distributor)
//   CPU enable (GICC_CTLR, priority mask GICC_PMR, Group 1 enable)
//      Must be done by each PE themself

void GIC::DriverV2::initialize() {
  initialize_gic_distributor();
}

void GIC::DriverV2::initialize_core_specific() {
  // Finish by enabling the CPU Interface. This is done PER-CORE and can not
  // be done solely by the "startup core"
  enable_cpu_interface();
  set_cpu_priority_mask(0xFF);
  enable_cpu_interrupts();
}

void GIC::DriverV2::initialize_interrupt(int id, InterruptPriority priority) {
  set_interrupt_priority(id, priority);
  set_interrupt_group(id);
  set_interrupt_routing(id);
  enable_interrupt(id);

  // TODO: Configure routing?
  // GIC::DriverV2::set_interrupt_routing(33);
}

uint32_t GIC::DriverV2::acknowledge_interrupt() {
  // Reading GICC_IAR returns the INTID of the highest-priority signaled
  // interrupt and drops it from the active list. A read of 1023 (spurious)
  // means there was no pending interrupt.
  return *cpu_interface(GICC_IAR);
}

void GIC::DriverV2::end_of_interrupt(uint32_t id) {
  // Writing the previously-acknowledged INTID to GICC_EOIR retires it.
  *cpu_interface(GICC_EOIR) = id;
  asm volatile("dsb sy" ::: "memory");
}
