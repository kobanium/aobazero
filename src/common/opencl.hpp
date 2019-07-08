// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#if defined(USE_OPENCL)
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace OCL {
  using uint = unsigned int;
  class Platform;
  class Platform_impl;
  class Program_impl;
  class Device_impl;
  class Queue_impl;
  class Memory_impl;
  class Kernel_impl;
  class Event_impl;
  
  class Event {
    std::unique_ptr<Event_impl> _impl;
  public:
    explicit Event();
    ~Event();
    Event(Event &&e);
    Event &operator=(Event &&event);
    Event_impl &get();
    const Event_impl &get() const;
    void wait() const;
  };

  class Memory {
    std::unique_ptr<Memory_impl> _impl;
  public:
    explicit Memory();
    ~Memory();
    Memory(Memory_impl &&m_impl);
    Memory(Memory &&m);
    Memory &operator=(Memory &&m);
    Memory_impl &get();
    const Memory_impl &get() const;
    bool ok() const;
  };

  class Kernel {
    std::unique_ptr<Kernel_impl> _impl;
  public:
    explicit Kernel();
    ~Kernel();
    Kernel(Kernel_impl &&k_impl);
    Kernel(Kernel &&k);
    Kernel &operator=(Kernel &&k);
    const Kernel_impl &get() const;
    bool ok() const;
    void set_arg(uint index, size_t size, const void *value) const;
    void set_arg(uint index, const Memory &m) const;
    std::string gen_info() const;
    size_t gen_work_group_size() const;
    uint64_t gen_local_mem_size() const;
    size_t gen_pref_wgs_multiple() const;
    uint64_t gen_private_mem_size() const;
  };

  class Program {
    std::unique_ptr<Program_impl> _impl;
  public:
    explicit Program();
    ~Program();
    Program(Program_impl &&p_impl);
    Program(Program &&p);
    Program &operator=(Program &&p);
    const Program_impl &get() const;
    bool ok() const;
    Kernel gen_kernel(const char *name) const;
  };

  class Queue {
    std::unique_ptr<Queue_impl> _impl;
  public:
    explicit Queue();
    ~Queue();
    Queue(Queue_impl &&q_impl);
    Queue(Queue &&q);
    Queue &operator=(Queue &&q);
    void reset();
    bool ok() const;
    void finish() const;
    Program gen_program(const char *code) const;
    Memory gen_mem_map_r(size_t size) const;
    Memory gen_mem_r(size_t size) const;
    Memory gen_mem_w(size_t size) const;
    Memory gen_mem_rw(size_t size) const;
    void *push_map_r(const Memory &m, size_t size) const;
    void *push_map_w(const Memory &m, size_t size) const;
    void push_unmap(const Memory &m, void *ptr) const;
    void push_write(const Memory &m, size_t size, const void *p) const;
    void push_read(const Memory &m, size_t size, void *p) const;
    void push_kernel(const Kernel &k, size_t size_global) const;
    void push_ndrange_kernel(const Kernel &k, uint dim, const size_t *size_g,
			     const size_t *size_l) const;
  };

  class Device {
    std::unique_ptr<Device_impl> _impl;
  public:
    explicit Device();
    ~Device();
    Device(Device_impl &&d_impl);
    Device(Device &&d);
    Device &operator=(Device &&d);
    Queue gen_queue() const;
    bool ok() const;
    std::string gen_info() const;
    std::string gen_type() const;
    std::string gen_local_mem_type() const;
    std::string gen_name() const;
    std::string gen_driver_version() const;
    Platform gen_platform() const;
    uint gen_max_compute_units() const;
    size_t gen_max_work_group_size() const;
    uint gen_max_clock_frequency() const;
    uint64_t gen_global_mem_size() const;
    uint64_t gen_max_mem_alloc_size() const;
    uint64_t gen_local_mem_size() const;
    size_t gen_kernel_preferred_multiple() const;
  };
    
  class Platform {
    std::unique_ptr<Platform_impl> _impl;
  public:
    explicit Platform();
    ~Platform();
    Platform(Platform_impl &&p_impl);
    Platform(Platform &&p);
    std::vector<Device> gen_devices_all() const;
    std::string gen_info() const;
    std::string gen_profile() const;
    std::string gen_version() const;
    std::string gen_name() const;
    std::string gen_extensions() const;
  };
  std::vector<Platform> gen_platforms();
}
#endif
