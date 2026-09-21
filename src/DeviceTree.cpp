
#include "DeviceTree.h"

#include <cstring>

#include "cpu.h"
#include "interrupts/gic.h"
#include "psci.h"
#include "uart.h"
#include "util/assert.h"
#include "memory/map.h"
#include "timer.h"

// Read a big-endian 32-bit integer from a byte pointer
static uint32_t be32(const void* p) {
  const uint8_t* b = static_cast<const uint8_t*>(p);
  return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) |
         (uint32_t(b[2]) << 8)  | uint32_t(b[3]);
}

// Read a big-endian 64-bit integer from a byte pointer
static uint64_t be64(const void* p) {
  return (uint64_t(be32(p)) << 32) | be32(static_cast<const uint8_t*>(p) + 4);
}

static const DeviceTree::NodeHandler _node_handlers[] = {
  { ._match_string = "arm,psci",           ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = PSCI::dt_parse        },
  { ._match_string = "arm,pl011",          ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = UART::dt_parse        },
  { ._match_string = "memory",             ._match_kind = DeviceTree::MatchKind::DeviceType, ._on_node = Memory::Map::dt_parse },
  { ._match_string = "cpu",                ._match_kind = DeviceTree::MatchKind::DeviceType, ._on_node = CPU::dt_parse         },
  { ._match_string = "arm,armv8-timer",    ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = Timer::dt_parse       },

  // Interrupt Controller
  { ._match_string = "arm,gic-400",        ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = GIC::dt_parse_v2      },
  { ._match_string = "arm,cortex-a15-gic", ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = GIC::dt_parse_v2      },
  { ._match_string = "arm,gic-v3",         ._match_kind = DeviceTree::MatchKind::Compatible, ._on_node = GIC::dt_parse_v3      },
};

static const uint32_t NumNodeHandlers = sizeof(_node_handlers) / sizeof(DeviceTree::NodeHandler);

void DeviceTree::Interrupts::register_intid(uint32_t intid) {
  kprecond(_num_intids < Capacity);
  _intids[_num_intids] = intid;
  _num_intids++;
}

uint32_t DeviceTree::Interrupts::get(uint32_t index) const {
  kprecond(index < _num_intids);
  return _intids[index];
}

void DeviceTree::Interrupts::reset() {
  _num_intids = 0;
}

void DeviceTree::read_reg_pair(const NodeCells* cells, const void* value, RegPair* out_rp) {
  const void* address_value = value;

  // Read address field
  uint64_t address = 0;
  if (cells->_address == 1) {
    address = DeviceTree::Parser::read_u32(address_value);
  } else if (cells->_address == 2) {
    address = DeviceTree::Parser::read_u64(address_value);
  }

  // Advance the value
  const void* length_value = (const char*)value + cells->_address * NodeCells::BytesPerUnit;

  // Read length field
  uint64_t length = 0;
  if (cells->_size == 0) {
    // Don't read anything
  } else if (cells->_size == 1) {
    length = DeviceTree::Parser::read_u32(length_value);
  } else if (cells->_size == 2) {
    length = DeviceTree::Parser::read_u64(length_value);
  }

  out_rp->_address = address;
  out_rp->_length = length;
}

uint32_t DeviceTree::read_interrupt_id(const PropFrame* prop, uint32_t index, uint32_t interrupt_cells) {
  const uintptr_t offset = index * (interrupt_cells * sizeof(uint32_t));
  const uintptr_t interrupt_value = reinterpret_cast<uintptr_t>(prop->_value) + offset;

  const uint32_t type = DeviceTree::Parser::read_u32(reinterpret_cast<void *>(interrupt_value));
  const uint32_t number = DeviceTree::Parser::read_u32(reinterpret_cast<void *>(interrupt_value + sizeof(uint32_t)));

  // flags = read_u32(... + 8);  // trigger type, ignore for now

  const uint32_t intid = type == 0
      ? 32 + number // SPI
      : 16 + number; // PPI

  return intid;
}

DeviceTree::Status DeviceTree::Parser::init(const FlattenedDeviceTree* fdt) {
  _valid = false;

  if (fdt == nullptr) {
    return Status::Malformed;
  }

  _header = reinterpret_cast<const FDTHeader*>(fdt);
  _blob = reinterpret_cast<const uint8_t*>(fdt);

  // Validate magic and version
  if (be32(&_header->_magic) != FDT_MAGIC) {
    return Status::BadMagic;
  }

  if (be32(&_header->_version) < FDT_SUPPORTED_VERSION) {
    return Status::UnsupportedVersion;
  }

  // Read header fields into host byte order
  const uint32_t totalsize = be32(&_header->_totalsize);
  const uint32_t off_struct = be32(&_header->_off_dt_struct);
  const uint32_t off_strings = be32(&_header->_off_dt_strings);
  const uint32_t off_rsvmap = be32(&_header->_off_mem_rsvmap);
  const uint32_t size_struct = be32(&_header->_size_dt_struct);
  const uint32_t size_strings = be32(&_header->_size_dt_strings);

  // Every block must fit entirely within the blob
  if (uint64_t(off_struct) + size_struct > totalsize ||
      uint64_t(off_strings) + size_strings > totalsize ||
      uint64_t(off_rsvmap) + 16 > totalsize) {
    return Status::Malformed;
  }

  _blob_end = _blob + totalsize;
  _struct = _blob + off_struct;
  _struct_end = _struct + size_struct;
  _strings = _blob + off_strings;
  _strings_size = size_strings;
  _pos = _struct;

  _token = Token::End;
  _depth = 0;
  _node_name = nullptr;
  _prop_name = nullptr;
  _prop_value = nullptr;
  _prop_len = 0;
  _valid = true;

  return Status::Ok;
}

uint32_t DeviceTree::Parser::totalsize() const {
  return (_header != nullptr) ? be32(&_header->_totalsize) : 0;
}

uint32_t DeviceTree::Parser::version() const {
  return (_header != nullptr) ? be32(&_header->_version) : 0;
}

uint32_t DeviceTree::Parser::last_comp_version() const {
  return (_header != nullptr) ? be32(&_header->_last_comp_version) : 0;
}

uint32_t DeviceTree::Parser::boot_cpuid_phys() const {
  return (_header != nullptr) ? be32(&_header->_boot_cpuid_phys) : 0;
}

bool DeviceTree::Parser::reserve_entry(uint32_t index, uint64_t* out_address, uint64_t* out_size) const {
  if (!_valid) {
    return false;
  }

  const uint8_t* p = _blob + be32(&_header->_off_mem_rsvmap) + 16 * index;
  if (p + 16 > _blob_end) {
    return false;
  }

  *out_address = be64(p);
  *out_size = be64(p + 8);

  // The list is terminated by an entry with both address and size set to 0
  return *out_address != 0 || *out_size != 0;
}

DeviceTree::Token DeviceTree::Parser::next() {
  if (!_valid || _pos >= _struct_end) {
    _token = Token::End;
    return _token;
  }

  for (;;) {
    if (_pos + 4 > _struct_end) {
      _token = Token::End;
      _pos   = _struct_end;
      return _token;
    }

    const uint32_t raw = be32(_pos);
    _pos += 4;

    switch (static_cast<Token>(raw)) {

      case Token::BeginNode: {
        // The node name is a null-terminated string at the current position
        const char* name = reinterpret_cast<const char*>(_pos);

        // Scan for the NUL terminator, bounded by the structure block end
        const char* scan = name;
        const char* end  = reinterpret_cast<const char*>(_struct_end);
        while (scan < end && *scan != '\0') {
          scan++;
        }
        if (scan >= end) {
          // Truncated node name — give up
          _token = Token::End;
          _pos   = _struct_end;
          return _token;
        }

        // Advance past the string and pad to a 4-byte boundary
        _pos       = align4(reinterpret_cast<const uint8_t*>(scan) + 1);
        _node_name = name;
        _depth++;
        _token = Token::BeginNode;
        return _token;
      }

      case Token::EndNode: {
        if (_depth > 0) {
          _depth--;
        }
        _token = Token::EndNode;
        return _token;
      }

      case Token::Prop: {
        if (_pos + 8 > _struct_end) {
          _token = Token::End;
          _pos   = _struct_end;
          return _token;
        }

        const uint32_t len = be32(_pos);
        const uint32_t nameoff = be32(_pos + 4);
        _pos += 8;

        const uint8_t* value = _pos;

        // Advance past the value and pad to a 4-byte boundary
        if (_pos + len > _struct_end) {
          _token = Token::End;
          _pos   = _struct_end;
          return _token;
        }
        _pos = align4(value + len);

        _prop_len   = len;
        _prop_value = value;

        // Resolve the property name from the strings block
        if (nameoff < _strings_size) {
          _prop_name = reinterpret_cast<const char*>(_strings + nameoff);
        } else {
          _prop_name = nullptr;
        }

        _token = Token::Prop;
        return _token;
      }

      case Token::Nop:
        // FDT_NOP tokens are ignored by any program parsing the tree
        continue;

      case Token::End:
        _pos   = _struct_end;
        _token = Token::End;
        return _token;

      // Unknown token value: stop parsing
      default:
        _pos   = _struct_end;
        _token = Token::End;
        return _token;
    }
  }
}

const char* DeviceTree::Parser::node_name() const {
  return _token == Token::BeginNode ? _node_name : nullptr;
}

const char* DeviceTree::Parser::prop_name() const {
  return _token == Token::Prop ? _prop_name : nullptr;
}

const void* DeviceTree::Parser::prop_value() const {
  return _token == Token::Prop ? _prop_value : nullptr;
}

uint32_t DeviceTree::Parser::prop_len() const {
  return _token == Token::Prop ? _prop_len : 0;
}

uint32_t DeviceTree::Parser::depth() const {
  return _depth;
}

uint32_t DeviceTree::Parser::read_u32(const void* p) {
  return be32(p);
}

uint64_t DeviceTree::Parser::read_u64(const void* p) {
  return be64(p);
}

uint64_t DeviceTree::Parser::read_prop(const DeviceTree::PropFrame* prop) {
  if (prop->_len == 4) {
    return read_u32(prop->_value);
  } else if (prop->_len == 8) {
    return read_u64(prop->_value);
  }

  kpanic("Bad prop length for Parser::read_prop");
  return 0;
}

// Returns true if any NULL-separated string in the "compatible" list equals
// needle. The list is value[0..len), where strings are separated by '\0';
// the final string may or may not be '\0'-terminated within len bytes.
static bool compatible_list_contains(const char* value, uint32_t len, const char* needle) {
  uint32_t offset = 0;
  while (offset < len) {
    const char* s = value + offset;  // start of this candidate
    uint32_t rest = len - offset;    // bytes remaining in buffer
    uint32_t slen = strnlen(s, rest); // candidate length, bounded by rest

    // Exact match against the handler name
    if (strlen(needle) == slen && strncmp(s, needle, slen) == 0) {
      return true;
    }

    offset += slen;
    if (offset < len && value[offset] == '\0') {
      offset++;
    }
  }

  return false;
}

static void dispatch_if_prop_match(const DeviceTree::NodeFrame* node_frame,
                                   const DeviceTree::PropFrame* compatible_prop,
                                   const DeviceTree::PropFrame* device_type_prop) {
  // Iterate over the NodeHandler entries in _node_handlers to see if this node
  // should be handled
  for (uint32_t i = 0; i < NumNodeHandlers; i++) {
    const DeviceTree::NodeHandler* handler = &_node_handlers[i];

    const DeviceTree::PropFrame* handler_match;
    switch (handler->_match_kind) {
      case DeviceTree::MatchKind::Compatible:
        handler_match = compatible_prop;
        break;
      case DeviceTree::MatchKind::DeviceType:
        handler_match = device_type_prop;
        break;
    }

    if (handler_match == nullptr) {
      continue;
    }

    if (compatible_list_contains(reinterpret_cast<const char*>(handler_match->_value),
                                 handler_match->_len,
                                 handler->_match_string)) {
      handler->_on_node(node_frame);
    }
  }
}

static const uint32_t PhandleNodeMappingCapacity = 300;
static uint32_t _num_phandle_to_node_mappings = 0;
static DeviceTree::NodeMapping _phandle_node_mapping[PhandleNodeMappingCapacity];

uint32_t DeviceTree::NodeCells::lookup_interrupt_cells() const {
  kprecond(_interrupt_parent != 0);

  for (uint32_t i = 0; i < _num_phandle_to_node_mappings; i++) {
    if (_phandle_node_mapping[i]._phandle == _interrupt_parent) {
      const uint32_t interrupt_cells = _phandle_node_mapping[i]._interrupt_cells;
      kprecond(interrupt_cells != 0);
      return interrupt_cells;
    }
  }

  kpanic("phandle %x cannot be found", _interrupt_parent);
  return 0;
}

static bool is_ignored_node(const char* node_name) {
  if (node_name == nullptr) {
    return false;
  }

  // Nodes starting with "__" are ignored
  if (node_name[0] == '_' && node_name[1] == '_') {
    return true;
  }

  // The "aliases" node is ignored
  if(strcmp(node_name, "aliases") == 0) {
    return true;
  }

  return false;
}

static DeviceTree::Status parse_frames_phandle(const DeviceTree::FlattenedDeviceTree* fdt) {
  DeviceTree::Parser dtp;

  DeviceTree::Status init_status = dtp.init(fdt);
  if (init_status != DeviceTree::Status::Ok) {
    return init_status;
  }

  // Reset the phandle to node mappings
  _num_phandle_to_node_mappings = 0;

  DeviceTree::ParsingFrame parsing_frame;
  parsing_frame._top = 0;

  while (dtp.next() != DeviceTree::Token::End) {
    switch (dtp.current()) {
      case DeviceTree::Token::BeginNode: {
        if (strcmp(dtp.node_name(), "") != 0) {
          // This is not the root node
          const DeviceTree::NodeCells parent_cells = parsing_frame.current()->_own_cells;

          parsing_frame._top++;
          kpostcond(parsing_frame._top < DeviceTree::MaxNodeDepth);

          parsing_frame.current()->_parent_cells = parent_cells;
        }

        parsing_frame.current()->_name = dtp.node_name();
        parsing_frame.current()->_nprops = 0;
        break;
      }
      case DeviceTree::Token::EndNode: {
        DeviceTree::NodeMapping mapping;

        // Find the prop values for the NodeMapping
        for (uint32_t i = 0; i < parsing_frame.current()->_nprops; i++) {
          const DeviceTree::PropFrame* prop = &parsing_frame.current()->_props[i];

          if (strcmp(prop->_name, "phandle") == 0) {
            mapping._phandle = DeviceTree::Parser::read_u32(prop->_value);
          } else if (strcmp(prop->_name, "#interrupt-cells") == 0) {
            mapping._interrupt_cells = DeviceTree::Parser::read_u32(prop->_value);
          }
        }

        if (mapping._phandle != 0) {
          _phandle_node_mapping[_num_phandle_to_node_mappings] = mapping;
          _num_phandle_to_node_mappings++;
          kpostcond(_num_phandle_to_node_mappings < PhandleNodeMappingCapacity);
        }

        // Move down in the parsing frame stack. The _top value is 0 if the node
        // end is for the root node.
        if (parsing_frame._top > 0) {
          parsing_frame._top--;
        }
      }
      break;
      case DeviceTree::Token::Prop:
        if (is_ignored_node(parsing_frame.current()->_name)) {
          continue;
        }

        // Store the prop data in the current slot
        parsing_frame.current()->current_prop()->_name = dtp.prop_name();
        parsing_frame.current()->current_prop()->_value = dtp.prop_value();
        parsing_frame.current()->current_prop()->_len = dtp.prop_len();

        parsing_frame.current()->_nprops++;
        kpostcond(parsing_frame.current()->_nprops < DeviceTree::MaxPropsPerNode);
        break;
      case DeviceTree::Token::End:
        break;
      case DeviceTree::Token::Nop:
        continue;
    }
  }

  return DeviceTree::Status::Ok;
}

static DeviceTree::Status parse_frames_full(const DeviceTree::FlattenedDeviceTree* fdt) {
  DeviceTree::Parser dtp;

  DeviceTree::Status init_status = dtp.init(fdt);
  if (init_status != DeviceTree::Status::Ok) {
    return init_status;
  }

  DeviceTree::ParsingFrame parsing_frame;
  parsing_frame._top = 0;

  while (dtp.next() != DeviceTree::Token::End) {
    switch (dtp.current()) {
      case DeviceTree::Token::BeginNode: {
        if (strcmp(dtp.node_name(), "") != 0) {
          // This is not the root node
          const DeviceTree::NodeCells parent_cells = parsing_frame.current()->_own_cells;

          parsing_frame._top++;
          kpostcond(parsing_frame._top < DeviceTree::MaxNodeDepth);

          parsing_frame.current()->_parent_cells = parent_cells;

          // Inherit interrupt parent
          parsing_frame.current()->_own_cells._interrupt_parent = parent_cells._interrupt_parent;
        }

        parsing_frame.current()->_name = dtp.node_name();
        parsing_frame.current()->_nprops = 0;
        break;
      }
      case DeviceTree::Token::EndNode: {
        const DeviceTree::PropFrame* compatible_prop = parsing_frame.current()->compatible_prop();
        const DeviceTree::PropFrame* device_type_prop = parsing_frame.current()->device_type_prop();

        dispatch_if_prop_match(parsing_frame.current(), compatible_prop, device_type_prop);

        // Move down in the parsing frame stack. The _top value is 0 if the node
        // end is for the root node.
        if (parsing_frame._top > 0) {
          parsing_frame._top--;
        }
      }
      break;
      case DeviceTree::Token::Prop:
        if (is_ignored_node(parsing_frame.current()->_name)) {
          continue;
        }

        if (strcmp(dtp.prop_name(), "#address-cells") == 0) {
          parsing_frame.current()->_own_cells._address = DeviceTree::Parser::read_u32(dtp.prop_value());
        } else if (strcmp(dtp.prop_name(), "#size-cells") == 0) {
          parsing_frame.current()->_own_cells._size = DeviceTree::Parser::read_u32(dtp.prop_value());
        } else if (strcmp(dtp.prop_name(), "interrupt-parent") == 0) {
          parsing_frame.current()->_own_cells._interrupt_parent = DeviceTree::Parser::read_u32(dtp.prop_value());
        } else {
          // Store the prop data in the current slot
          parsing_frame.current()->current_prop()->_name = dtp.prop_name();
          parsing_frame.current()->current_prop()->_value = dtp.prop_value();
          parsing_frame.current()->current_prop()->_len = dtp.prop_len();

          // Check if the current prop is one that we should keep track of for
          // matching handlers with
          if (strcmp(dtp.prop_name(), "compatible") == 0) {
            parsing_frame.current()->_compatible_prop_idx = parsing_frame.current()->_nprops;
          } else if (strcmp(dtp.prop_name(), "device_type") == 0) {
            parsing_frame.current()->_device_type_prop_idx = parsing_frame.current()->_nprops;
          }

          parsing_frame.current()->_nprops++;
          kpostcond(parsing_frame.current()->_nprops < DeviceTree::MaxPropsPerNode);
        }
        break;
      case DeviceTree::Token::End:
        break;
      case DeviceTree::Token::Nop:
        continue;
    }
  }

  return DeviceTree::Status::Ok;
}

DeviceTree::Status DeviceTree::parse_frames(const DeviceTree::FlattenedDeviceTree* fdt) {
  // The first pass gathers information that needs to be available for the second
  // pass to be able to stream parsing.
  Status status = parse_frames_phandle(fdt);
  if (status != Status::Ok) {
    return status;
  }

  // The second pass does the actual work of gathering the bulk of information
  return parse_frames_full(fdt);
}
