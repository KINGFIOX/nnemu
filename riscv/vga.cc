#include "devices.h"
#include <cstring>
#include <stdexcept>

vga_t::vga_t(int width, int height)
  : width_(width), height_(height)
{
  fb_size_ = (size_t)width * height * sizeof(uint32_t);
  if (fb_size_ > VGA_SIZE)
    throw std::runtime_error("VGA framebuffer exceeds device region size");
  pixels_ = new uint32_t[width * height]();
}

vga_t::~vga_t()
{
  delete[] pixels_;
}

bool vga_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  if (addr + len > fb_size_)
    return false;

  memcpy(bytes, reinterpret_cast<uint8_t*>(pixels_) + addr, len);
  return true;
}

bool vga_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  if (addr + len > fb_size_)
    return false;

  memcpy(reinterpret_cast<uint8_t*>(pixels_) + addr, bytes, len);
  return true;
}
