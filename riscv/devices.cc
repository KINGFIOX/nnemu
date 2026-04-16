#include "devices.h"
#include "mmu.h"
#include <stdexcept>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

void bus_t::add_device(reg_t addr, abstract_device_t* dev)
{
  // Searching devices via lower_bound/upper_bound
  // implicitly relies on the underlying std::map 
  // container to sort the keys and provide ordered
  // iteration over this sort, which it does. (python's
  // SortedDict is a good analogy)
  devices[addr] = dev;
}

bool bus_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  // Find the device with the base address closest to but
  // less than addr (price-is-right search)
  auto it = devices.upper_bound(addr);
  if (devices.empty() || it == devices.begin()) {
    // Either the bus is empty, or there weren't 
    // any items with a base address <= addr
    return false;
  }
  // Found at least one item with base address <= addr
  // The iterator points to the device after this, so
  // go back by one item.
  it--;
  return it->second->load(addr - it->first, len, bytes);
}

bool bus_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  // See comments in bus_t::load
  auto it = devices.upper_bound(addr);
  if (devices.empty() || it == devices.begin()) {
    return false;
  }
  it--;
  return it->second->store(addr - it->first, len, bytes);
}

std::pair<reg_t, abstract_device_t*> bus_t::find_device(reg_t addr)
{
  // See comments in bus_t::load
  auto it = devices.upper_bound(addr);
  if (devices.empty() || it == devices.begin()) {
    return std::make_pair((reg_t)0, (abstract_device_t*)NULL);
  }
  it--;
  return std::make_pair(it->first, it->second);
}

// Type for holding all registered MMIO plugins by name.
using mmio_plugin_map_t = std::map<std::string, mmio_plugin_t>;

// Simple singleton instance of an mmio_plugin_map_t.
static mmio_plugin_map_t& mmio_plugin_map()
{
  static mmio_plugin_map_t instance;
  return instance;
}

void register_mmio_plugin(const char* name_cstr,
                          const mmio_plugin_t* mmio_plugin)
{
  std::string name(name_cstr);
  if (!mmio_plugin_map().emplace(name, *mmio_plugin).second) {
    throw std::runtime_error("Plugin \"" + name + "\" already registered!");
  }
}

mmio_plugin_device_t::mmio_plugin_device_t(const std::string& name,
                                           const std::string& args)
  : plugin(mmio_plugin_map().at(name)), user_data((*plugin.alloc)(args.c_str()))
{
}

mmio_plugin_device_t::~mmio_plugin_device_t()
{
  (*plugin.dealloc)(user_data);
}

bool mmio_plugin_device_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  return (*plugin.load)(user_data, addr, len, bytes);
}

bool mmio_plugin_device_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  return (*plugin.store)(user_data, addr, len, bytes);
}

mem_t::mem_t(reg_t size)
  : sz(size)
{
  if (size == 0 || size % PGSIZE != 0)
    throw std::runtime_error("memory size must be a positive multiple of 4 KiB");
}

mem_t::~mem_t()
{
  for (auto& entry : sparse_memory_map)
    free(entry.second);
}

bool mem_t::load_store(reg_t addr, size_t len, uint8_t* bytes, bool store)
{
  if (addr + len < addr || addr + len > sz)
    return false;

  while (len > 0) {
    auto n = std::min(PGSIZE - (addr % PGSIZE), reg_t(len));

    if (store)
      memcpy(this->contents(addr), bytes, n);
    else
      memcpy(bytes, this->contents(addr), n);

    addr += n;
    bytes += n;
    len -= n;
  }

  return true;
}

char* mem_t::contents(reg_t addr) {
  reg_t ppn = addr >> PGSHIFT, pgoff = addr % PGSIZE;
  auto search = sparse_memory_map.find(ppn);
  if (search == sparse_memory_map.end()) {
    auto res = (char*)calloc(PGSIZE, 1);
    if (res == nullptr)
      throw std::bad_alloc();
    sparse_memory_map[ppn] = res;
    return res + pgoff;
  }
  return search->second + pgoff;
}

void mem_t::dump(std::ostream& o) {
  const char empty[PGSIZE] = {0};
  for (reg_t i = 0; i < sz; i += PGSIZE) {
    reg_t ppn = i >> PGSHIFT;
    auto search = sparse_memory_map.find(ppn);
    if (search == sparse_memory_map.end()) {
      o.write(empty, PGSIZE);
    } else {
      o.write(sparse_memory_map[ppn], PGSIZE);
    }
  }
}

sync_disk_t::sync_disk_t(class bus_t* bus, const std::string& image_path)
  : bus(bus), fd(-1), cmd(CMD_NONE), status(STATUS_IDLE), count(0), reserved(0),
    lba_low(0), lba_high(0), guest_pa_low(0), guest_pa_high(0),
    error_code(0), last_result_bytes(0)
{
  fd = open(image_path.c_str(), O_RDWR, 0);
  if (fd < 0) {
    throw std::runtime_error("failed to open fs image '" + image_path + "': " + strerror(errno));
  }
}

sync_disk_t::~sync_disk_t()
{
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

uint64_t sync_disk_t::lba() const
{
  return (uint64_t(lba_high) << 32) | lba_low;
}

uint64_t sync_disk_t::guest_pa() const
{
  return (uint64_t(guest_pa_high) << 32) | guest_pa_low;
}

bool sync_disk_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  if (addr + len < addr || addr + len > SYNC_DISK_SIZE) {
    return false;
  }

  switch (addr & ~reg_t(0x3)) {
    case 0x00:
      read_little_endian_reg(cmd, addr, len, bytes);
      return true;
    case 0x04:
      read_little_endian_reg(status, addr, len, bytes);
      return true;
    case 0x08:
      read_little_endian_reg(count, addr, len, bytes);
      return true;
    case 0x0c:
      read_little_endian_reg(reserved, addr, len, bytes);
      return true;
    case 0x10:
      read_little_endian_reg(lba_low, addr, len, bytes);
      return true;
    case 0x14:
      read_little_endian_reg(lba_high, addr, len, bytes);
      return true;
    case 0x18:
      read_little_endian_reg(guest_pa_low, addr, len, bytes);
      return true;
    case 0x1c:
      read_little_endian_reg(guest_pa_high, addr, len, bytes);
      return true;
    case 0x20:
      read_little_endian_reg(error_code, addr, len, bytes);
      return true;
    case 0x24:
      read_little_endian_reg(last_result_bytes, addr, len, bytes);
      return true;
    default:
      return false;
  }
}

bool sync_disk_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  if (addr + len < addr || addr + len > SYNC_DISK_SIZE) {
    return false;
  }

  switch (addr & ~reg_t(0x3)) {
    case 0x00:
      write_little_endian_reg(&cmd, addr, len, bytes);
      if (cmd == CMD_NONE) {
        status = STATUS_IDLE;
        error_code = 0;
        return true;
      }
      return execute_command();
    case 0x04:
      write_little_endian_reg(&status, addr, len, bytes);
      if (status == STATUS_IDLE) {
        error_code = 0;
      }
      return true;
    case 0x08:
      write_little_endian_reg(&count, addr, len, bytes);
      return true;
    case 0x0c:
      write_little_endian_reg(&reserved, addr, len, bytes);
      return true;
    case 0x10:
      write_little_endian_reg(&lba_low, addr, len, bytes);
      return true;
    case 0x14:
      write_little_endian_reg(&lba_high, addr, len, bytes);
      return true;
    case 0x18:
      write_little_endian_reg(&guest_pa_low, addr, len, bytes);
      return true;
    case 0x1c:
      write_little_endian_reg(&guest_pa_high, addr, len, bytes);
      return true;
    default:
      return false;
  }
}

bool sync_disk_t::execute_command()
{
  static constexpr uint32_t kSectorSize = 512;

  if (count == 0) {
    status = STATUS_DONE;
    error_code = 0;
    last_result_bytes = 0;
    cmd = CMD_NONE;
    return true;
  }
  if (cmd != CMD_READ && cmd != CMD_WRITE) {
    status = STATUS_ERROR;
    error_code = EINVAL;
    last_result_bytes = 0;
    cmd = CMD_NONE;
    return false;
  }

  const uint64_t total_bytes = uint64_t(count) * kSectorSize;
  if (total_bytes > UINT32_MAX) {
    status = STATUS_ERROR;
    error_code = EINVAL;
    last_result_bytes = 0;
    cmd = CMD_NONE;
    return false;
  }

  std::vector<uint8_t> buffer(total_bytes);
  const off_t offset = static_cast<off_t>(lba() * kSectorSize);

  if (cmd == CMD_READ) {
    const ssize_t got = pread(fd, buffer.data(), buffer.size(), offset);
    if (got < 0) {
      status = STATUS_ERROR;
      error_code = errno;
      last_result_bytes = 0;
      cmd = CMD_NONE;
      return false;
    }
    if (static_cast<size_t>(got) < buffer.size()) {
      memset(buffer.data() + got, 0, buffer.size() - got);
    }
    if (!bus->store(guest_pa(), buffer.size(), buffer.data())) {
      status = STATUS_ERROR;
      error_code = EFAULT;
      last_result_bytes = 0;
      cmd = CMD_NONE;
      return false;
    }
    last_result_bytes = buffer.size();
  } else {
    if (!bus->load(guest_pa(), buffer.size(), buffer.data())) {
      status = STATUS_ERROR;
      error_code = EFAULT;
      last_result_bytes = 0;
      cmd = CMD_NONE;
      return false;
    }
    const ssize_t wrote = pwrite(fd, buffer.data(), buffer.size(), offset);
    if (wrote < 0 || static_cast<size_t>(wrote) != buffer.size()) {
      status = STATUS_ERROR;
      error_code = (wrote < 0) ? errno : EIO;
      last_result_bytes = 0;
      cmd = CMD_NONE;
      return false;
    }
    last_result_bytes = buffer.size();
  }

  status = STATUS_DONE;
  error_code = 0;
  cmd = CMD_NONE;
  return true;
}
