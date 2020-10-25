// 2019 Team AobaZero
// This source code is in the public domain.
#pragma once
#if defined(USE_OPENCL_AOBA)
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
  class Context_impl;
  class Memory_impl;
  class MemPinned_impl;
  class Kernel_impl;
  
  class Memory {
    std::unique_ptr<Memory_impl> _impl;
  public:
    explicit Memory();
    ~Memory();
    Memory(Memory_impl &&m_impl);
    Memory(Memory &&m);
    Memory &operator=(Memory &&m);
    void clear();
    const Memory_impl &get() const;
    bool ok() const; };

  class MemPinned {
    std::unique_ptr<MemPinned_impl> _impl;
  public:
    explicit MemPinned();
    ~MemPinned();
    MemPinned(MemPinned_impl &&m_impl);
    MemPinned(MemPinned &&m);
    MemPinned &operator=(MemPinned &&m);
    void clear();
    const MemPinned_impl &get() const;
    void *get_pointer() const;
    bool ok() const; };

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
    template <typename M> void set_arg(uint index, const M &m) const;
    std::string gen_info() const;
    size_t gen_work_group_size() const;
    uint64_t gen_local_mem_size() const;
    size_t gen_pref_wgs_multiple() const;
    uint64_t gen_private_mem_size() const; };

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
    Kernel gen_kernel(const char *name) const; };

  class Queue {
    std::unique_ptr<Queue_impl> _impl;
  public:
    explicit Queue();
    ~Queue();
    Queue(Queue_impl &&q_impl);
    Queue(Queue &&q);
    Queue &operator=(Queue &&q);
    bool ok() const;
    void finish() const;
    void push_write(const MemPinned &m, size_t size) const;
    void push_write(const Memory &m, size_t size, const void *p) const;
    void push_read(const MemPinned &m, size_t size) const;
    void push_kernel(const Kernel &k, size_t size_global) const;
    void push_ndrange_kernel(const Kernel &k, uint dim, const size_t *size_g,
			     const size_t *size_l) const; };

  class Context {
    std::unique_ptr<Context_impl> _impl;
  public:
    explicit Context();
    ~Context();
    Context(Context_impl &&q_impl);
    Context(Context &&q);
    Context &operator=(Context &&q);
    bool ok() const;
    Queue gen_queue() const;
    Program gen_program(const char *code, bool flag_aggressive = true) const;
    Program gen_program(const std::string &code,
			bool flag_aggressive = true) const;
    MemPinned gen_mem_pin_hw_dr(size_t size) const;
    MemPinned gen_mem_pin_hr_dw(size_t size) const;
    Memory gen_mem_hw_dr(size_t size) const;
    Memory gen_mem_hr_dw(size_t size) const;
    Memory gen_mem_drw(size_t size) const; };

  class Device {
    std::unique_ptr<Device_impl> _impl;
  public:
    explicit Device();
    explicit Device(Device_impl &&d_impl);
    ~Device();
    Device(Device &&d);
    Device &operator=(Device &&d);
    Context gen_context() const;
    bool ok() const;
    std::string gen_info() const;
    std::string gen_type() const;
    std::string gen_local_mem_type() const;
    std::string gen_name() const;
    std::string gen_driver_version() const;
    double evaluation() const;
    Platform gen_platform() const;
    uint gen_max_compute_units() const;
    size_t gen_max_work_group_size() const;
    uint gen_max_clock_frequency() const;
    uint64_t gen_global_mem_size() const;
    uint64_t gen_max_mem_alloc_size() const;
    uint64_t gen_local_mem_size() const;
    size_t gen_kernel_preferred_multiple() const; };
    
  class Platform {
    std::unique_ptr<Platform_impl> _impl;
  public:
    explicit Platform();
    ~Platform();
    Platform(Platform_impl &&p_impl);
    Platform(Platform &&p);
    Platform &operator=(Platform &&p);
    bool ok() const;
    std::vector<Device> gen_devices_all() const;
    std::string gen_info() const;
    std::string gen_profile() const;
    std::string gen_version() const;
    std::string gen_name() const;
    std::string gen_extensions() const; };

  std::vector<Platform> gen_platforms();
}
#endif
