// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace OCL {
  using uint = unsigned int;
  class Platform;
  class Platform_impl;
  class Device_impl;
  class Memory_impl;
  class Kernel_impl;
  class Event_impl;

  class Events {
    uint _num;
    std::unique_ptr<Event_impl []> _impl;
  public:
    explicit Events() noexcept;
    ~Events() noexcept;
    explicit Events(uint u) noexcept;
    Events(Events &&event) noexcept;
    Events &operator=(Events &&event) noexcept;
    Event_impl &get(uint u) noexcept;
    const Event_impl &get(uint u) const noexcept;
    void wait(uint u) const noexcept;
  };

  class Memory {
    std::unique_ptr<Memory_impl> _impl;
  public:
    explicit Memory() noexcept;
    ~Memory() noexcept;
    Memory(Memory_impl &&m_impl) noexcept;
    Memory(Memory &&m) noexcept;
    Memory &operator=(Memory &&m) noexcept;
    const Memory_impl &get() const noexcept;
    bool ok() const noexcept;
  };

  class Kernel {
    std::unique_ptr<Kernel_impl> _impl;
  public:
    explicit Kernel() noexcept;
    ~Kernel() noexcept;
    Kernel(Kernel_impl &&k_impl) noexcept;
    Kernel(Kernel &&k) noexcept;
    Kernel &operator=(Kernel &&k) noexcept;
    bool ok() const noexcept;
    void set_arg(uint index, size_t size, const void *value) const noexcept;
    void set_arg(uint index, const Memory &m) const noexcept;
    std::string gen_info() const noexcept;
    size_t gen_work_group_size() const noexcept;
    uint64_t gen_local_mem_size() const noexcept;
    size_t gen_pref_wgs_multiple() const noexcept;
    uint64_t gen_private_mem_size() const noexcept;
  };
  
  class Device {
    std::unique_ptr<Device_impl> _impl;
  public:
    explicit Device() noexcept;
    ~Device() noexcept;
    Device(Device_impl &&d_impl) noexcept;
    Device(Device &&d) noexcept;
    Device &operator=(Device &&d) noexcept;
    void build_program(const char *code) noexcept;
    Memory gen_memory_r(size_t size) const noexcept;
    Memory gen_memory_w(size_t size) const noexcept;
    Memory gen_memory_rw(size_t size) const noexcept;
    Kernel gen_kernel(const char *name) const noexcept;
    void push_write(const Memory &m, size_t size, const float *p,
		    Event_impl &e_impl) const noexcept;
    void push_read(const Memory &m, size_t size, float *p,
		   const Event_impl &event_wait, Event_impl &event)
      const noexcept;
    bool ok() const noexcept;
    std::string gen_info() const noexcept;
    std::string gen_type() const noexcept;
    std::string gen_name() const noexcept;
    std::string gen_driver_version() const noexcept;
    Platform gen_platform() const noexcept;
    uint gen_max_compute_units() const noexcept;
    size_t gen_max_work_group_size() const noexcept;
    uint gen_max_clock_frequency() const noexcept;
    uint64_t gen_global_mem_size() const noexcept;
    uint64_t gen_max_mem_alloc_size() const noexcept;
    uint64_t gen_local_mem_size() const noexcept;
    size_t gen_kernel_preferred_multiple() const noexcept;
  };
    
  class Platform {
    std::unique_ptr<Platform_impl> _impl;
  public:
    explicit Platform() noexcept;
    ~Platform() noexcept;
    Platform(Platform_impl &&p_impl) noexcept;
    Platform(Platform &&p) noexcept;
    std::vector<Device> gen_devices_all() const noexcept;
    std::string gen_info() const noexcept;
    std::string gen_profile() const noexcept;
    std::string gen_version() const noexcept;
    std::string gen_name() const noexcept;
    std::string gen_extensions() const noexcept;
  };
  std::vector<Platform> gen_platforms() noexcept;
}
