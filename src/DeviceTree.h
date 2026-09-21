#ifndef INCLUDE_DEVICE_TREE
#define INCLUDE_DEVICE_TREE

#include <cstdint>

// From QEMU virt documentation https://www.qemu.org/docs/master/system/arm/virt.html
//
// The virt board automatically generates a device tree blob ("dtb") which it
// passes to the guest. This provides information about the addresses, interrupt
// lines and other configuration of the various devices in the system. Guest code
// can rely on and hard-code the following addresses:
//
// Flash memory starts at address 0x0000_0000
//   The flash memory contains memory mapped (device) registers (MMIO)
//   Area 0x0000_0000 to 0x3FFF_FFFF
//
// RAM starts at 0x4000_0000
//   The area 0x4000_0000 - 0x4007_FFFF is reserved for bootloader
//   Kernel is loaded at 0x4008_0000
//
// All other information about device locations may change between QEMU versions,
// so guest code must look in the DTB.

namespace DeviceTree {

  class Interrupts {
  public:
    // More than four intids are ever (rarely?) needed
    static const uint32_t Capacity = 4;

  private:
    uint32_t _num_intids = 0;
    uint32_t _intids[Capacity];

  public:
    void register_intid(uint32_t intid);
    uint32_t get(uint32_t index) const;

    void reset();
  };

  // Gives a total static allocation size of:
  // MaxNodeDepth * sizeof(PropFrame) * MaxPropsPerNode
  const uint32_t MaxPropsPerNode = 30;
  const uint32_t MaxNodeDepth    = 8;

  struct PropFrame {
    const char* _name;
    const void* _value;
    uint32_t    _len;
  };

  // Holds the values that should transitively propogate with Nodes.
  // See chapter 2.3.5 for some details on this.
  struct NodeCells {
    // Each unit is worth four bytes / size of uint32_t
    static const uint32_t BytesPerUnit = sizeof(uint32_t);

    uint32_t _address{2};
    uint32_t _size{1};

    // A phandle value to a NodeMapping
    uint32_t _interrupt_parent{0};

    inline uint32_t total() const { return _address + _size; }
    inline uint32_t byte_size() const { return total() * BytesPerUnit; }

    uint32_t lookup_interrupt_cells() const;
  };

  struct NodeFrame {
    const char* _name;
    NodeCells   _parent_cells;
    NodeCells   _own_cells;
    uint32_t    _nprops;
    int         _compatible_prop_idx{-1};
    int         _device_type_prop_idx{-1};
    PropFrame   _props[MaxPropsPerNode];

    inline PropFrame* current_prop() { return &_props[_nprops]; }
    inline PropFrame* compatible_prop() { return _compatible_prop_idx < 0 ? nullptr : &_props[_compatible_prop_idx]; }
    inline PropFrame* device_type_prop() { return _device_type_prop_idx < 0 ? nullptr : &_props[_device_type_prop_idx]; }
  };

  struct ParsingFrame {
    NodeFrame _node_frame[MaxNodeDepth];
    uint32_t  _top; // The current NodeFrame position

    inline NodeFrame* current() { return &_node_frame[_top]; }
  };

  struct NodeMapping {
    uint32_t  _phandle{0};

    // We might want to add more information here in the future. Instances
    // will only track some of the fields below.
    uint32_t _interrupt_cells{0};
  };

  enum class MatchKind {
    Compatible,
    DeviceType,
  };

  struct NodeHandler {
    const char*     _match_string;
    const MatchKind _match_kind;
    void (*_on_node)(const DeviceTree::NodeFrame*);
  };

  // A pair of (address, length) values stored in the "reg" property of a node
  // See spec chapter 2.3.6
  struct RegPair {
    uint64_t _address;
    uint64_t _length;
  };

  void read_reg_pair(const NodeCells* cells, const void* value, RegPair* out_rp);
  uint32_t read_interrupt_id(const PropFrame* prop, uint32_t index, uint32_t interrupt_cells);

  // Chapter 5 of the Devicetree Specification v0.4
  // Raw flattened device tree (FDT) header, as laid out in memory (big-endian)
  struct FDTHeader {
    uint32_t _magic;             // Always 0xd00dfeed (big-endian)
    uint32_t _totalsize;         // Total blob size in bytes
    uint32_t _off_dt_struct;     // Offset of the structure block from the blob start
    uint32_t _off_dt_strings;    // Offset of the strings block
    uint32_t _off_mem_rsvmap;    // Offset of the memory reservation block
    uint32_t _version;           // 17 for the layout described by this spec
    uint32_t _last_comp_version; // Lowest backwards-compatible version (16)
    uint32_t _boot_cpuid_phys;   // Physical ID of the boot CPU
    uint32_t _size_dt_strings;   // Size of the strings block in bytes
    uint32_t _size_dt_struct;    // Size of the structure block in bytes
  };

  // A flattened device tree blob in memory. The header occupies the first bytes;
  // every other block (memory reservations, structure, strings) is located via
  // offsets in the header. Because FDTHeader is the first member, a pointer to
  // a FlattenedDeviceTree can be reinterpret_cast to const FDTHeader*.
  typedef FDTHeader FlattenedDeviceTree;

  // Structure block token values (big-endian 32-bit integers)
  enum class Token : uint32_t {
    BeginNode = 0x00000001,
    EndNode   = 0x00000002,
    Prop      = 0x00000003,
    Nop       = 0x00000004,
    End       = 0x00000009,
  };

  enum class Status {
    Ok,
    BadMagic,          // Blob does not start with Parser::FDT_MAGIC
    UnsupportedVersion,
    Malformed,         // Invalid offsets, sizes, or truncated data
  };

  // Zero-allocation streaming parser over a flattened device tree blob
  //
  // Validates the header and walks the structure block token by token, reading
  // node names and property data directly out of the blob (no copying, no heap
  // allocation). The blob must remain valid for the lifetime of the parser.
  class Parser {
  private:
    static constexpr uint32_t FDT_MAGIC   = 0xD00DFEED;
    static constexpr uint32_t FDT_SUPPORTED_VERSION = 17;

    // Advance a pointer past the next 4-byte boundary
    static const uint8_t* align4(const uint8_t* p) {
      const uintptr_t m = reinterpret_cast<uintptr_t>(p) & 3;
      return p + (m ? 4 - m : 0);
    }

    const FDTHeader* _header{nullptr};
    const uint8_t*   _blob{nullptr};
    const uint8_t*   _blob_end{nullptr};
    const uint8_t*   _struct{nullptr};       // start of structure block
    const uint8_t*   _struct_end{nullptr};   // end of structure block
    const uint8_t*   _strings{nullptr};      // start of strings block
    uint32_t         _strings_size{0};
    const uint8_t*   _pos{nullptr};          // next token position
    Token            _token{Token::End};
    const char*      _node_name{nullptr};
    const char*      _prop_name{nullptr};
    const void*      _prop_value{nullptr};
    uint32_t         _prop_len{0};
    uint32_t         _depth{0};
    bool             _valid{false};

  public:
    // Validate the blob and prepare for walking. The blob must be 8-byte
    // aligned (as required by the spec)
    Status init(const FlattenedDeviceTree* fdt);

    uint32_t totalsize() const;
    uint32_t version() const;
    uint32_t last_comp_version() const;
    uint32_t boot_cpuid_phys() const;

    // Fetches the index-th reservation entry (physical address, size in bytes)
    // Returns false once the terminating (0, 0) entry is reached, or if index
    // is out of range
    bool reserve_entry(uint32_t index, uint64_t* out_address, uint64_t* out_size) const;

    // Advances to the next token and returns it. FDT_NOP tokens are skipped
    // automatically. Returns Token::End once the structure block is exhausted
    // (and on any subsequent call). The first call returns the root node's
    // BeginNode
    Token next();

    // The token returned by the most recent next() call
    Token current() const { return _token; }

    // Data for the current token (valid after next() returns a non-End token)
    const char* node_name() const;   // Token::BeginNode: node's unit name ("" for root)
    const char* prop_name() const;   // Token::Prop:      property name (strings block)
    const void* prop_value() const;  // Token::Prop:      raw value bytes (big-endian)
    uint32_t    prop_len() const;    // Token::Prop:      value length in bytes
    uint32_t    depth() const;       // Number of open BeginNode tokens (0 = at root level)

    // Read a big-endian integer from an arbitrary byte pointer (e.g., a property
    // value). Returns the value in host byte order.
    static uint32_t read_u32(const void* p);
    static uint64_t read_u64(const void* p);
    static uint64_t read_prop(const PropFrame* prop);
  };

  Status parse_frames(const FlattenedDeviceTree* fdt);

} // namespace DeviceTree

#endif // INCLUDE_DEVICE_TREE
