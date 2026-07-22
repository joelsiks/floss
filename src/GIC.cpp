
#include "GIC.h"

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

// Memory Mapped Device Register for the Distributor
struct MMDR_GICD {
  volatile uint32_t CTLR;
  volatile uint32_t TYPER;
  volatile uint32_t IIDR;
};

// Memory Mapped Device Register for the Redistributor
struct MMDR_GICR_RD {
  volatile uint32_t CTRL;
  volatile uint32_t IIDR;
  volatile uint64_t TYPER;
  volatile uint32_t STATUSR;
  volatile uint32_t WAKER;
};

// TODO: THis should really be found using the Device Tree
static MMDR_GICD* gicd = reinterpret_cast<MMDR_GICD*>(0x08000000);

static uintptr_t GICR_BASE = 0x080A0000;
static uintptr_t GICR_STRIDE = 0x20000;
static uintptr_t GICR_SD_OFFSET = 0;
static uintptr_t GICR_SGI_OFFSET = 0x10000;

static uintptr_t GICR_SGI_IGROUPR0 = 0x80;
static uintptr_t GICR_SGI_ISENABLER0 = 0x100;
static uintptr_t GICR_SGI_IPRIORITYRN_BASE = 0x400; // + 4 * n

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
  uint32_t ctrl = gicd->CTLR;
  ctrl |= (GICD_CTLR_Group1NS | GICD_CTLR_Group0);
  gicd->CTLR = ctrl;

  // Ensure write to GICD_CTLR has completed before continuing
  asm volatile ("dsb sy" ::: "memory");

  // This blog post checks if the ARE_S field is set in the CTRL register. Maybe
  // we should check the ARE_NS field if we care about that?
  // https://jcomes.org/aarch64-os-interrupt-handling-ii
}

static const uint32_t GICR_WAKER_ProcessorSleep = 0b010;
static const uint32_t GICR_WAKER_ChildrenAsleep = 0b100;

void GIC::v3::initialize_gic_redistributor() {
  // Enable each core's Redistributor. By default, the Redistributor is in a
  // low-power state to conserve energy. The Redistributor is awoken by clearing
  // the ProcessorSleep bit in the GICR_WAKER register.
  //
  // TODO: Right now we don't have any methods for setting the priority via the
  // GICR_IPRIORITYR<n> register(s)

  // Read-Modify-Write
  uint32_t waker = redistributor_rd(0)->WAKER;
  waker &= ~GICR_WAKER_ProcessorSleep;
  redistributor_rd(0)->WAKER = waker;

  // Ensure write to GICR_WAKER has completed before continuing
  asm volatile ("dsb sy" ::: "memory");

  // Busy-wait until the ChildrenAsleep bit becomes 0, indicating that the
  // Redistributor has awoken
  while ((redistributor_rd(0)->WAKER & GICR_WAKER_ChildrenAsleep) != 0) { }
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

  volatile uint32_t* p = redistributor_sgi(0, GICR_SGI_IPRIORITYRN_BASE + reg_offset);

  // Read-Modify-Write
  uint32_t ipriorityn = *p;
  ipriorityn = (ipriorityn & ~(0xFF << shift)) | (priority << shift);
  *p = ipriorityn;
}

void GIC::v3::set_interrupt_group(int id) {
  // Read-Modify-Write
  uint32_t igroupr0 = *redistributor_sgi(0, GICR_SGI_IGROUPR0);
  igroupr0 |= 1 << id;
  *redistributor_sgi(0, GICR_SGI_IGROUPR0) = igroupr0;
  asm volatile("dsb sy" ::: "memory");
}

void GIC::v3::enable_interrupt(int id) {
  const uint32_t enable_bit = 1 << id;
  *redistributor_sgi(0, GICR_SGI_ISENABLER0) = enable_bit;
  asm volatile("dsb sy" ::: "memory");
}

void GIC::initialize() {
  GIC::v3::initialize_gic_distributor();
  GIC::v3::initialize_gic_redistributor();

  GIC::v3::enable_cpu_interface();
  GIC::v3::set_cpu_priority_mask(0xFF);
  GIC::v3::enable_cpu_interrupts();

  GIC::v3::set_interrupt_priority(30, 90);
  GIC::v3::set_interrupt_group(30);
  GIC::v3::enable_interrupt(30);

}
