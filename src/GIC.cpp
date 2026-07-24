
#include "GIC.h"
#include "kstdio.h"

// The Distributor, Redistributor, and Interrupt Translation Service (ITS) are
// collectively known as an Interrupt Routing Infrastructure (IRI). There is one
// single Distributor, one Redistributor for every Processing Element (CPU/core),
// and one single ITS.
//
// Some abbreviations:
// SPI - Shared Peripheral Interrupt (sent to the Distributor)
// PPI - Private Peripheral Interrupt (sent to a Redistributor)
// SGI - Software Generated Interrupt (sent from the PE to the Redistributor)
// LPI - Locality-specific Peripheral Interrupt (sent from the ITS)
//
// SGI and PPI are handled by a Redistributor and have interrupt ids (INTID)
// of 0-31. SGI (and LPI?) are handled by the Distributor and have INTIDs of
// 32-1023 (maybe more?)

// Memory Mapped Device Register for the Distributor
struct MMDR_GICD {
  volatile uint32_t CTLR;
  volatile uint32_t TYPER;
  volatile uint32_t IIDR;
};

static const uintptr_t GICD_BASE = 0x08000000;
static const uintptr_t GICD_IGROUPR = 0x80;
static const uintptr_t GICD_ISENABLER = 0x100;

static const uintptr_t GICD_IPRIORITY_BASE = 0x400;

static volatile uint32_t* distributor(uintptr_t offset = 0) {
  return reinterpret_cast<volatile uint32_t*>(GICD_BASE + offset);
}

// TODO: This should really be found using the Device Tree
static MMDR_GICD* const gicd = reinterpret_cast<MMDR_GICD*>(GICD_BASE);

// Memory Mapped Device Register for the Redistributor
struct MMDR_GICR_RD {
  volatile uint32_t CTRL;
  volatile uint32_t IIDR;
  volatile uint64_t TYPER;
  volatile uint32_t STATUSR;
  volatile uint32_t WAKER;
};

static const uintptr_t GICR_BASE = 0x080A0000;
static const uintptr_t GICR_STRIDE = 0x20000;
static const uintptr_t GICR_SD_OFFSET = 0;
static const uintptr_t GICR_SGI_OFFSET = 0x10000;

static const uintptr_t GICR_SGI_IGROUPR0 = 0x80;
static const uintptr_t GICR_SGI_ISENABLER0 = 0x100;
static const uintptr_t GICR_SGI_IPRIORITYR_BASE = 0x400; // + 4 * n

static uint32_t NumRedistributors = 0;

static MMDR_GICR_RD* redistributor_rd(int n) {
  return reinterpret_cast<MMDR_GICR_RD*>(GICR_BASE + n * GICR_STRIDE + GICR_SD_OFFSET);
}

static volatile uint32_t* redistributor_sgi(int n, uintptr_t offset = 0) {
  return reinterpret_cast<volatile uint32_t*>(GICR_BASE + n * GICR_STRIDE + GICR_SD_OFFSET + GICR_SGI_OFFSET + offset);
}

static const uint32_t GICD_CTLR_Group0   = 0b01;
static const uint32_t GICD_CTLR_Group1NS = 0b10;

void GIC::v3::initialize_gic_distributor() {
  // Read-Modify-Write so that we're not overwriting any other "feature" bits with 0.
  uint32_t ctlr = gicd->CTLR;
  ctlr |= (GICD_CTLR_Group1NS | GICD_CTLR_Group0);
  gicd->CTLR = ctlr;

  // Ensure write to GICD_CTLR has completed before continuing
  asm volatile ("dsb sy" ::: "memory");

  // This blog post checks if the ARE_S field is set in the CTRL register. Maybe
  // we should check the ARE_NS field if we care about that?
  // https://jcomes.org/aarch64-os-interrupt-handling-ii
}

static const uint32_t GICR_TYPER_Last = 0b10000;
static const uint32_t GICR_WAKER_ProcessorSleep = 0b010;
static const uint32_t GICR_WAKER_ChildrenAsleep = 0b100;

void GIC::v3::initialize_gic_redistributors() {
  // Enable each core's Redistributor. By default, the Redistributor is in a
  // low-power state to conserve energy. The Redistributor is awoken by clearing
  // the ProcessorSleep bit in the GICR_WAKER register.
  uint32_t current_redistributor = 0;

  for (;;) {
    // Read-Modify-Write
    uint32_t waker = redistributor_rd(current_redistributor)->WAKER;
    waker &= ~GICR_WAKER_ProcessorSleep;
    redistributor_rd(current_redistributor)->WAKER = waker;

    // Ensure write to GICR_WAKER has completed before continuing
    asm volatile ("dsb sy" ::: "memory");

    // Busy-wait until the ChildrenAsleep bit becomes 0, indicating that the
    // Redistributor has awoken
    while ((redistributor_rd(current_redistributor)->WAKER & GICR_WAKER_ChildrenAsleep) != 0) { }

    if ((redistributor_rd(current_redistributor)->TYPER & GICR_TYPER_Last) != 0) {
      // If the "last" bit is set in the TYPER mmdr, this distributor is the last one
      break;
    }

    // Move on to next redistributor
    current_redistributor++;
  }

  NumRedistributors = current_redistributor + 1;

  kprintf("Num redistributors: %d\n", NumRedistributors);
}

void GIC::v3::enable_cpu_interface() {
  // ICC_SRE_EL1, Interrupt Controller System Register Enable Register (EL1)
  asm volatile(
   "mrs   x0, ICC_SRE_EL1 \n\t\
    orr   x0, x0, #1      \n\t\
    msr   ICC_SRE_EL1, x0 \n\t\
    isb                   \n\t\
    "
    ::: "memory"
  );
}
void GIC::v3::enable_cpu_interrupts() {
  // ICC_IGRPEN1_EL1, Interrupt Controller Interrupt Group 1 Enable Register
  asm volatile(
   "mrs   x0, ICC_IGRPEN1_EL1 \n\t\
    orr   x0, x0, #1          \n\t\
    msr   ICC_IGRPEN1_EL1, x0 \n\t\
    isb                       \n\t\
    "
    ::: "memory"
  );
}

void GIC::v3::set_cpu_priority_mask(uint64_t priority) {
  asm volatile("msr ICC_PMR_EL1, %0" :: "r"(priority));
}

void GIC::v3::set_interrupt_priority(int id, uint8_t priority) {
  const uint32_t reg_index = id / 4;
  const uint32_t reg_offset = reg_index * 4;
  const uint32_t byte_index = id % 4;
  const uint32_t shift = byte_index * 8;

  if (id > 31) {
    volatile uint32_t* p = distributor(GICD_IPRIORITY_BASE + reg_offset);
    // Read-Modify-Write
    uint32_t ipriorityn = *p;
    ipriorityn = (ipriorityn & ~(0xFF << shift)) | (priority << shift);
    *p = ipriorityn;
    asm volatile("dsb sy" ::: "memory");
  } else {
    for (uint32_t i = 0; i < NumRedistributors; i++) {
      // Read-Modify-Write
      volatile uint32_t* p = redistributor_sgi(i, GICR_SGI_IPRIORITYR_BASE + reg_offset);
      uint32_t ipriorityn = *p;
      ipriorityn = (ipriorityn & ~(0xFF << shift)) | (priority << shift);
      *p = ipriorityn;
    }
    asm volatile("dsb sy" ::: "memory");
  }
}

void GIC::v3::set_interrupt_group(int id) {
  if (id > 31) {
    const uint32_t reg_index = id / 32;
    const uint32_t reg_offset = reg_index * 4;
    const uint32_t shift = id % 32;

    // Read-Modify-Write
    uint32_t igrouprn = *distributor(GICD_IGROUPR + reg_offset);
    igrouprn |= (1 << shift);
    *distributor(GICD_IGROUPR + reg_offset) = igrouprn;
    asm volatile("dsb sy" ::: "memory");
  } else {
    for (uint32_t i = 0; i < NumRedistributors; i++) {
      // Read-Modify-Write
      uint32_t igroupr0 = *redistributor_sgi(i, GICR_SGI_IGROUPR0);
      igroupr0 |= 1 << id;
      *redistributor_sgi(i, GICR_SGI_IGROUPR0) = igroupr0;
    }
    asm volatile("dsb sy" ::: "memory");
  }
}

void GIC::v3::enable_interrupt(int id) {
  // Both the GICD_ISENABLER<n> and GICR_ISENABLER0 are "write-1-to-set",
  // so no masking is required to not affect other bits. ICENABLER has the
  // opposite effect of "write-1-to-clear".

  if (id > 31) {
    const uint32_t reg_index = id / 32;
    const uint32_t reg_offset = reg_index * 4;
    const uint32_t shift = id % 32;

    *distributor(GICD_ISENABLER + reg_offset) = (1 << shift);
    asm volatile("dsb sy" ::: "memory");
  } else {
    // The Redistributor only handles interrupt ids between 0-31, which are the
    // SGI and PPI ids
    for (uint32_t i = 0; i < NumRedistributors; i++) {
      *redistributor_sgi(i, GICR_SGI_ISENABLER0) = (1 << id);
    }
    asm volatile("dsb sy" ::: "memory");
  }
}

void GIC::v3::disable_interrupt(int id) {
  (void)id;
  // TODO: Implement via the GICD_ICENABLER<n>/GICR_ICENABLER0, "write-1-to-clear"
}

// Initialization sequence:
//   GICD (Distributor)
//   GICR (Redistributor)
//   Setup interrupt ids (priority, group, then enable)
//   CPU enable (ICC_SRE_EL1, priority mask ICC_PMR_EL1, ICC_IGRPEN1_EL1)
//      Must be done by each PE themself

void GIC::initialize() {
  GIC::v3::initialize_gic_distributor();
  GIC::v3::initialize_gic_redistributors();

  // TODO: Better priority for these interrupts?
  GIC::v3::set_interrupt_priority(30, 90);
  GIC::v3::set_interrupt_group(30);
  GIC::v3::enable_interrupt(30);

  GIC::v3::set_interrupt_priority(33, 90);
  GIC::v3::set_interrupt_group(33);
  GIC::v3::enable_interrupt(33);

  initialize_core_specific();
}

void GIC::initialize_core_specific() {
  // Finish by enabling interrupts in the CPU interface. This is done PER-CORE
  // and can not be done solely by the "startup core"
  GIC::v3::enable_cpu_interface();
  GIC::v3::set_cpu_priority_mask(0xFF);
  GIC::v3::enable_cpu_interrupts();
}
