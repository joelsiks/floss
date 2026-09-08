
#include "cpu.h"

#include <cstring>

static uint32_t _num_cpu_cores = 0;

void CPU::dt_parse(const DeviceTree::NodeFrame*) {
  _num_cpu_cores++;
}

uint32_t CPU::num_cpu_cores() {
  return _num_cpu_cores;
}
