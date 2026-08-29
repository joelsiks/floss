
#include "GIC.h"

#include <cstring>

#include "DeviceTree.h"
#include "kstdio.h"
#include "util/assert.h"

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

static uintptr_t GICD_BASE = 0;

// Memory Mapped Device Registers for the Distributor
static const uintptr_t GICD_CTLR = 0x0000;
static const uintptr_t GICD_TYPER = 0x0004;
static const uintptr_t GICD_IGROUPR = 0x80;
static const uintptr_t GICD_ISENABLER = 0x100;

static const uintptr_t GICD_IPRIORITY_BASE = 0x400;
static const uintptr_t GICD_IROUTER = 0x6000;

static volatile uint32_t* distributor(uintptr_t offset) {
  return reinterpret_cast<volatile uint32_t*>(GICD_BASE + offset);
}

static volatile uint64_t* distributor_wide(uintptr_t offset) {
  return reinterpret_cast<volatile uint64_t*>(GICD_BASE + offset);
}

static uint32_t NumRedistributors = 0;
static uintptr_t GICR_BASE = 0;

static const uintptr_t GICR_STRIDE = 0x20000;
static const uintptr_t GICR_RD_OFFSET = 0;
static const uintptr_t GICR_SGI_OFFSET = 0x10000; // 64 KB

// Memory Mapped Device Registers for the Redistributors
static const uintptr_t GICR_RD_CTRL = 0x000;
static const uintptr_t GICR_RD_IIDR = 0x004;
static const uintptr_t GICR_RD_TYPER = 0x008;
static const uintptr_t GICR_RD_STATUSR = 0x010;
static const uintptr_t GICR_RD_WAKER = 0x014;

static const uintptr_t GICR_SGI_IGROUPR0 = 0x080;
static const uintptr_t GICR_SGI_ISENABLER0 = 0x100;
static const uintptr_t GICR_SGI_IPRIORITYR_BASE = 0x400; // + 4 * n

static volatile uint32_t* redistributor_rd(int n, uintptr_t offset) {
  return reinterpret_cast<volatile uint32_t*>(GICR_BASE + n * GICR_STRIDE + GICR_RD_OFFSET + offset);
}

static volatile uint64_t* redistributor_rd_wide(int n, uintptr_t offset) {
  return reinterpret_cast<volatile uint64_t*>(GICR_BASE + n * GICR_STRIDE + GICR_RD_OFFSET + offset);
}

static volatile uint32_t* redistributor_sgi(int n, uintptr_t offset) {
  return reinterpret_cast<volatile uint32_t*>(GICR_BASE + n * GICR_STRIDE + GICR_SGI_OFFSET + offset);
}

void GIC::v3::dt_parse(const DeviceTree::NodeFrame* node_frame) {
  // Iterate over all the props
  for (uint32_t i = 0; i < node_frame->_nprops; i++) {
    const DeviceTree::PropFrame* prop = &node_frame->_props[i];

    if (strcmp(prop->_name, "reg") == 0) {
      const void* current_value = prop->_value;
      const uint32_t rp_size_bytes = node_frame->_parent_cells.byte_size();
      const uint32_t num_reg_pairs = prop->_len / rp_size_bytes;

      kassert(prop->_len % rp_size_bytes == 0, "Invalid reg length (%d, rp size %d)\n", prop->_len, rp_size_bytes);

      DeviceTree::RegPair rp;
      for (uint32_t j = 0; j < num_reg_pairs; j++) {
        DeviceTree::read_reg_pair(&node_frame->_parent_cells, current_value, &rp);
        if (j == 0) {
          GICD_BASE = reinterpret_cast<uintptr_t>(rp._address);
        } else if (j == 1) {
          GICR_BASE = reinterpret_cast<uintptr_t>(rp._address);
        }

        current_value = (const char*)current_value + rp_size_bytes;
      }
    }
  }
}

// Single security state view
static const uint32_t GICD_CTLR_EnableGrp0 = 0b01;
static const uint32_t GICD_CTLR_EnableGrp1 = 0b10;

// Two security states non-secure view
static const uint32_t GICD_CTLR_NS_EnableGrp1 = 0b1;
static const uint32_t GICD_CTLR_NS_EnableGrp1A = 0b10;

static const uint32_t GICD_CTLR_DS = 1 << 6;
static const uint32_t GICD_CTLR_E1NWF = 1 << 6; // Enable 1 of N Wakeup Functionality
static const uint32_t GICD_CTLR_RWP = 1u << 31;

void GIC::v3::initialize_gic_distributor() {
  uint32_t ctlr = *distributor(GICD_CTLR);

  const bool ds = (ctlr & GICD_CTLR_DS) != 0;
  if (ds) {
    // If the DS bit is set to 1, then the system supports only a single
    // security state.
    ctlr |= (GICD_CTLR_E1NWF | GICD_CTLR_EnableGrp1);
  } else {
    // If the DS bit is set to 0, then the system supports two security states.
    // We know floss runs in non-secure, so use those toggles.
    ctlr |= (GICD_CTLR_E1NWF | GICD_CTLR_NS_EnableGrp1A);
  }

  *distributor(GICD_CTLR) = ctlr;

  // Ensure write to GICD_CTLR has completed before continuing
  while ((*distributor(GICD_CTLR) & GICD_CTLR_RWP) != 0) { }
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
    uint32_t waker = *redistributor_rd(current_redistributor, GICR_RD_WAKER);
    waker &= ~GICR_WAKER_ProcessorSleep;
    *redistributor_rd(current_redistributor, GICR_RD_WAKER) = waker;

    // Ensure write to GICR_WAKER has completed before continuing
    asm volatile ("dsb sy" ::: "memory");

    // Busy-wait until the ChildrenAsleep bit becomes 0, indicating that the
    // Redistributor has awoken
    while ((*redistributor_rd(current_redistributor, GICR_RD_WAKER) & GICR_WAKER_ChildrenAsleep) != 0) { }

    if ((*redistributor_rd_wide(current_redistributor, GICR_RD_TYPER) & GICR_TYPER_Last) != 0) {
      // If the "last" bit is set in TYPER, this distributor is the last one
      break;
    }

    // Move on to the next redistributor
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
  // From ARM: Architectural execution of a DSB instruction guarantees that: The
  // last value written to ICC_PMR_EL1 is observed by the associated Redistributor.
  asm volatile(
   "msr ICC_PMR_EL1, %0 \n\t\
    dsb sy              \n\t\
    isb                 \n\t\
    "
    :: "r"(priority) : "memory");
}

void GIC::v3::set_interrupt_priority(int id, uint8_t priority) {
  if (id > 31) {
    // SPI, LPI
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(GICD_BASE + GICD_IPRIORITY_BASE + id);
    *p = priority;
  } else {
    // SGI/PPI: per-CPU, in the Redistributor's SGI frame
    for (uint32_t i = 0; i < NumRedistributors; i++) {
      volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(
          GICR_BASE + i * GICR_STRIDE + GICR_RD_OFFSET + GICR_SGI_OFFSET + GICR_SGI_IPRIORITYR_BASE + id);
      *p = priority;
    }
  }

  asm volatile("dsb sy" ::: "memory");
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
  } else {
    for (uint32_t i = 0; i < NumRedistributors; i++) {
      // Read-Modify-Write
      uint32_t igroupr0 = *redistributor_sgi(i, GICR_SGI_IGROUPR0);
      igroupr0 |= 1 << id;
      *redistributor_sgi(i, GICR_SGI_IGROUPR0) = igroupr0;
    }
  }

  asm volatile("dsb sy" ::: "memory");
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

static const uint64_t GICD_IROUTER_IRM = (uint64_t)1 << 63; // Interrupt Routing Mode

void GIC::v3::set_interrupt_routing(int id, bool any) {
  if (id > 31)  {
    const uint64_t offset = 8 * id;

    uint64_t iroutern = *distributor_wide(GICD_IROUTER + offset);

    if (any) {
      // Setting the IRM bit to 1 means that his INTID is forwarded to any PE
      // that qualify as a "participating node". A participating node is a PE
      // that has:
      //    GICR_WAKER.ProcessorSleep == 0 (with INTID configured on that Redistributor)
      //    GICD_CTLR.E1NWF == 1
      //    GICR_TYPER.DPGS == 1 // Disable Processor Group Selections
      iroutern |= GICD_IROUTER_IRM;
    } else {
      // TODO: Handle this case, which involves setting the affinity (aff0-aff3)
    }

    // Write value
    *distributor_wide(GICD_IROUTER + offset) = iroutern;
    asm volatile("dsb sy" ::: "memory");
  } else {
  }
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

  GIC::v3::set_interrupt_routing(33, true);

  initialize_core_specific();
}

void GIC::initialize_core_specific() {
  // Finish by enabling interrupts in the CPU interface. This is done PER-CORE
  // and can not be done solely by the "startup core"
  GIC::v3::enable_cpu_interface();
  GIC::v3::set_cpu_priority_mask(0xFF);
  GIC::v3::enable_cpu_interrupts();
}
