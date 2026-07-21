
#include <cstdint>

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
struct MMDR_GICR {
  volatile uint32_t CTRL;
  volatile uint32_t IIDR;
  volatile uint64_t TYPER;
  volatile uint32_t STATUSR;
  volatile uint32_t WAKER;
};

// TODO: These should really be found using the Device Tree
static MMDR_GICD* gicd = reinterpret_cast<MMDR_GICD*>(0x08000000);
static MMDR_GICR* gicr_pe0 = reinterpret_cast<MMDR_GICR*>(0x080A0000);

static const uint32_t GICD_CTLR_Group0   = 0b01;
static const uint32_t GICD_CTLR_Group1NS = 0b10;

void GIC::init_gic_distributor() {
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

void GIC::init_gic_redistributor() {
  // Enable each core's Redistributor. By default, the Redistributor is in a
  // low-power state to conserve energy. The Redistributor is awoken by clearing
  // the ProcessorSleep bit in the GICR_WAKER register.

  // TODO: Right now we're only enabling the first core's Redistributor. Enable
  // the Redistributor for other cores as well?
  //
  // TODO: Right now we don't have any methods for setting the priority via the
  // GICR_IPRIORITYR<n> register(s)

  // Read-Modify-Write
  uint32_t waker = gicr_pe0->WAKER;
  waker &= !GICR_WAKER_ProcessorSleep;
  gicr_pe0->WAKER = waker;

  // Ensure write to GICR_WAKER has completed before continuing
  asm volatile ("dsb sy" ::: "memory");

  // Busy-wait until the ChildrenAsleep bit becomes 0, indicating that the
  // Redistributor has awoken
  while ((gicr_pe0->WAKER & GICR_WAKER_ChildrenAsleep) != 0) { }
}
