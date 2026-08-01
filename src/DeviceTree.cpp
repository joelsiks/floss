#include "DeviceTree.h"

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

DeviceTree::Status DeviceTree::Parser::init(const FlattenedDeviceTree* fdt) {
  _valid = false;

  if (fdt == nullptr) {
    return Status::Malformed;
  }

  _header = reinterpret_cast<const FDTHeader*>(fdt);
  _blob = reinterpret_cast<const uint8_t*>(fdt);

  // Validate magic and version
  if (be32(&_header->magic) != FDT_MAGIC) {
    return Status::BadMagic;
  }

  if (be32(&_header->version) < FDT_SUPPORTED_VERSION) {
    return Status::UnsupportedVersion;
  }

  // Read header fields into host byte order
  const uint32_t totalsize = be32(&_header->totalsize);
  const uint32_t off_struct = be32(&_header->off_dt_struct);
  const uint32_t off_strings = be32(&_header->off_dt_strings);
  const uint32_t off_rsvmap = be32(&_header->off_mem_rsvmap);
  const uint32_t size_struct = be32(&_header->size_dt_struct);
  const uint32_t size_strings = be32(&_header->size_dt_strings);

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
  return (_header != nullptr) ? be32(&_header->totalsize) : 0;
}

uint32_t DeviceTree::Parser::version() const {
  return (_header != nullptr) ? be32(&_header->version) : 0;
}

uint32_t DeviceTree::Parser::last_comp_version() const {
  return (_header != nullptr) ? be32(&_header->last_comp_version) : 0;
}

uint32_t DeviceTree::Parser::boot_cpuid_phys() const {
  return (_header != nullptr) ? be32(&_header->boot_cpuid_phys) : 0;
}

bool DeviceTree::Parser::reserve_entry(uint32_t index, uint64_t& address, uint64_t& size) const {
  if (!_valid) {
    return false;
  }

  const uint8_t* p = _blob + be32(&_header->off_mem_rsvmap) + 16 * index;
  if (p + 16 > _blob_end) {
    return false;
  }

  address = be64(p);
  size = be64(p + 8);

  // The list is terminated by an entry with both address and size set to 0
  return address != 0 || size != 0;
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

        const uint32_t len     = be32(_pos);
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


