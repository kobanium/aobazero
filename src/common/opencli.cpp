#if defined(USE_OPENCL_AOBA)
#include "err.hpp"
#include "opencli.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <cstdint>
#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif
using uint = unsigned int;
using std::lock_guard;
using std::map;
using std::move;
using std::mutex;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace OCL;
using namespace ErrAux;

constexpr char options[]            = "-Werror -cl-std=CL1.2";
constexpr char options_aggressive[] = "-Werror -cl-std=CL1.2 "
                                      "-cl-fast-relaxed-math";

class OCL::Memory_impl {
  cl_mem _mem;
public:
  explicit Memory_impl(const cl_context &context, const cl_mem_flags &flags,
		       size_t size) {
    assert(context);
    cl_int ret;
    _mem = clCreateBuffer(context, flags, size, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateBuffer() failed. Error Code: %d", ret); }
  Memory_impl(Memory_impl &&m_impl) { *this = move(m_impl); }
  Memory_impl &operator=(Memory_impl &&m_impl) {
    assert(m_impl.ok());
    if (this != &m_impl) { _mem = m_impl._mem; m_impl._mem = nullptr; }
    return *this; }
  ~Memory_impl() { if (_mem) clReleaseMemObject(_mem); }
  const cl_mem *get_p() const { return &_mem; }
  cl_mem get() const { return _mem; }
  bool ok() const { return _mem; } };

OCL::Memory::Memory() {}
OCL::Memory::~Memory() {}
OCL::Memory::Memory(Memory_impl &&m_impl)
  : _impl(new Memory_impl(move(m_impl))) {}
OCL::Memory::Memory(Memory &&m) { *this = move(m); }
Memory &OCL::Memory::operator=(Memory &&m) {
  assert(m.ok()); if (this != &m) _impl = move(m._impl); return *this; }
void OCL::Memory::clear() { _impl.reset(nullptr); }
bool OCL::Memory::ok() const { return _impl && _impl->ok(); }
const Memory_impl &OCL::Memory::get() const { return *_impl; }

class OCL::MemPinned_impl {
  cl_context _context;
  cl_device_id _id;
  cl_mem _mem_device, _mem_pinned;
  void *_pointer;
public:
  explicit MemPinned_impl(const cl_context &context, const cl_device_id &id,
			  const cl_mem_flags &mem_flags,
			  const cl_map_flags &map_flags, size_t size)
    : _context(context), _id(id) {
    assert(context && id);
    cl_int ret;
    _mem_pinned = clCreateBuffer(context, mem_flags | CL_MEM_ALLOC_HOST_PTR,
				 size, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateBuffer() failed. Error Code: %d", ret);

    _mem_device = clCreateBuffer(context, mem_flags, size, nullptr, &ret);
    if (ret != CL_SUCCESS) {
      clReleaseMemObject(_mem_pinned);
      throw ERR_INT("clCreateBuffer() failed. Error Code: %d", ret); }

    cl_command_queue queue = clCreateCommandQueue(context, id, 0, &ret);
    if (ret != CL_SUCCESS) {
      clReleaseMemObject(_mem_pinned);
      clReleaseMemObject(_mem_device);
      throw ERR_INT("clCreateCommandQueue() failed. Error Code: %d", ret); }

    _pointer = clEnqueueMapBuffer(queue, _mem_pinned, CL_TRUE, map_flags, 0,
				  size, 0, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) {
      clReleaseCommandQueue(queue);
      clReleaseMemObject(_mem_pinned);
      clReleaseMemObject(_mem_device);
      throw ERR_INT("clEnqueueMapBuffer() failed. Error Code: %d", ret); }

    if (clReleaseCommandQueue(queue) != CL_SUCCESS) {
      clReleaseMemObject(_mem_pinned);
      clReleaseMemObject(_mem_device);
      throw ERR_INT("clReleaseCommandQueue() failed. Error Code: %d", ret); } }
  MemPinned_impl(MemPinned_impl &&m_impl) { *this = move(m_impl); }
  MemPinned_impl &operator=(MemPinned_impl &&m_impl) {
    assert(m_impl.ok());
    if (this != &m_impl) {
      _context    = m_impl._context;
      _id         = m_impl._id;
      _mem_device = m_impl._mem_device;
      _mem_pinned = m_impl._mem_pinned;
      _pointer    = m_impl._pointer;
      m_impl._id  = nullptr; }
    return *this; }
  ~MemPinned_impl() {
    if (! _id) return;
    cl_int ret;
    cl_command_queue queue = clCreateCommandQueue(_context, _id, 0, &ret);
    if (ret != CL_SUCCESS) return;

    if (clEnqueueUnmapMemObject(queue, _mem_pinned, _pointer, 0, nullptr,
				nullptr) != CL_SUCCESS) return;
    if (clReleaseCommandQueue(queue) != CL_SUCCESS) return;
    if (clReleaseMemObject(_mem_device) != CL_SUCCESS) return;
    if (clReleaseMemObject(_mem_pinned) != CL_SUCCESS) return; }
  const cl_mem *get_p() const { return &_mem_device; }
  cl_mem get() const { return _mem_device; }
  bool ok() const { return (_mem_device && _mem_pinned && _pointer); }
  void *get_pointer() const { return _pointer; } };

OCL::MemPinned::MemPinned() {}
OCL::MemPinned::~MemPinned() {}
OCL::MemPinned::MemPinned(MemPinned_impl &&m_impl)
  : _impl(new MemPinned_impl(move(m_impl))) {}
OCL::MemPinned::MemPinned(MemPinned &&m) { *this = move(m); }
MemPinned &OCL::MemPinned::operator=(MemPinned &&m) {
  assert(m.ok()); if (this != &m) _impl = move(m._impl); return *this; }
void OCL::MemPinned::clear() { _impl.reset(nullptr); }
bool OCL::MemPinned::ok() const { return _impl && _impl->ok(); }
const MemPinned_impl &OCL::MemPinned::get() const { return *_impl; }
void *OCL::MemPinned::get_pointer() const { return _impl->get_pointer(); }

class OCL::Kernel_impl {
  cl_kernel _kernel;
public:
  explicit Kernel_impl(const cl_program &program, const char *name) {
    assert(program && name);
    cl_int ret;
    _kernel = clCreateKernel(program, name, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateKernel() failed. Error Code %d", ret); }
  Kernel_impl(Kernel_impl &&k_impl) { *this = move(k_impl); }
  Kernel_impl &operator=(Kernel_impl &&k_impl) {
    assert(k_impl.ok());
    if (this != &k_impl) {
      _kernel = k_impl._kernel;
      k_impl._kernel = nullptr; }
    return *this; }
  ~Kernel_impl() { if (_kernel) clReleaseKernel(_kernel); }
  cl_kernel get() const { return _kernel; }
  void set_arg(uint index, size_t size, const void *value) const {
    assert(value);
    cl_int ret = clSetKernelArg(_kernel, index, size, value);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clSetKernelArg() failed. Error Code: %d", ret); }
  template <typename M> void set_arg(uint index, const M &m_impl) const {
    assert(m_impl.ok()); set_arg(index, sizeof(cl_mem), m_impl.get_p()); }
  bool ok() const { return _kernel; }
  template <typename T>
  T gen_info(const cl_kernel_work_group_info &name) const {
    T value;
    cl_int ret = clGetKernelWorkGroupInfo(_kernel, nullptr, name, sizeof(T),
					  &value, nullptr);
    if (ret != CL_SUCCESS) throw ERR_INT("clGetKernelWorkGroupInfo() failed.");
    return value; } };

OCL::Kernel::Kernel() {}
OCL::Kernel::~Kernel() {}
OCL::Kernel::Kernel(Kernel_impl &&k_impl)
  : _impl(new Kernel_impl(move(k_impl))) {}
OCL::Kernel::Kernel(Kernel &&k) { *this = move(k); }
Kernel &OCL::Kernel::operator=(Kernel &&k) {
  assert(k.ok()); if (this != &k) _impl = move(k._impl); return *this; }
const Kernel_impl &OCL::Kernel::get() const { return *_impl; }
bool OCL::Kernel::ok() const { return _impl && _impl->ok(); }
void OCL::Kernel::set_arg(uint index, size_t size, const void *value) const {
  assert(value); return _impl->set_arg(index, size, value); }
template <typename M> void OCL::Kernel::set_arg(uint index, const M &m) const {
  assert(m.ok()); return _impl->set_arg(index, m.get()); }
template void OCL::Kernel::set_arg<>(uint index, const Memory &m) const;
template void OCL::Kernel::set_arg<>(uint index, const MemPinned &m) const;
string OCL::Kernel::gen_info() const {
  stringstream ss;
  ss << "    Max Work-Group Size: " << gen_work_group_size()   << "\n"
     << "    Local Mem Size:      " << gen_local_mem_size()    << "\n"
     << "    Preferred WGS multi: " << gen_pref_wgs_multiple() << "\n"
     << "    private mem size:    " << gen_private_mem_size()  << "\n";
  return ss.str(); }
size_t OCL::Kernel::gen_work_group_size() const {
  return _impl->gen_info<size_t>(CL_KERNEL_WORK_GROUP_SIZE); }
cl_ulong OCL::Kernel::gen_local_mem_size() const {
  return _impl->gen_info<cl_ulong>(CL_KERNEL_LOCAL_MEM_SIZE); }
size_t OCL::Kernel::gen_pref_wgs_multiple() const {
  return
    _impl->gen_info<size_t>(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE); }
cl_ulong OCL::Kernel::gen_private_mem_size() const {
  return _impl->gen_info<cl_ulong>(CL_KERNEL_PRIVATE_MEM_SIZE); }

class OCL::Program_impl {
  cl_program _pg;
public:
  explicit Program_impl(cl_program pg) : _pg(pg) {
    assert(pg);
    cl_int ret = clRetainProgram(_pg);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clRetainProgram() failed. Error Code: %d", ret); }

  Program_impl(Program_impl &&p_impl) { *this = move(p_impl); }
  Program_impl &operator=(Program_impl &&p_impl) {
    assert(p_impl.ok());
    if (this != &p_impl) { _pg = p_impl._pg; p_impl._pg = nullptr; }
    return *this; }
  ~Program_impl() { clReleaseProgram(_pg); }
  bool ok() const { return _pg; }
  Kernel_impl gen_kernel(const char *name) const {
    assert(name); return Kernel_impl(_pg, name); } };

OCL::Program::Program() {}
OCL::Program::~Program() {}
OCL::Program::Program(Program_impl &&p_impl)
  : _impl(new Program_impl(move(p_impl))) {}
OCL::Program::Program(Program &&p) { *this = move(p); }
Program &OCL::Program::operator=(Program &&p) {
  assert(p.ok()); if (this != &p) _impl = move(p._impl); return *this; }
const Program_impl &OCL::Program::get() const { return *_impl; }
bool OCL::Program::ok() const { return _impl && _impl->ok(); }
Kernel OCL::Program::gen_kernel(const char *name) const {
  assert(name); return Kernel(_impl->gen_kernel(name)); }

class OCL::Queue_impl {
  cl_command_queue _queue;
public:
  Queue_impl(const cl_device_id &id, const cl_context &context) {
    assert(id && context);
    cl_int ret;
    _queue = clCreateCommandQueue(context, id, 0, &ret);
     if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateCommandQueue() failed. Error Code: %d", ret); }
  Queue_impl(Queue_impl &&q_impl) { *this = move(q_impl); }
  Queue_impl &operator=(Queue_impl &&q_impl) {
    assert(q_impl.ok());
    if (this != &q_impl) {
      _queue = q_impl._queue;
      q_impl._queue = nullptr; }
    return *this; }
  ~Queue_impl() {
    if (! _queue) return;
    clFinish(_queue);
    clReleaseCommandQueue(_queue); }
  const cl_command_queue &get() const { return _queue; }
  bool ok() const { return _queue; }
  void finish() const {
    cl_int ret = clFinish(_queue);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clFinish() failure. Error Code: %d", ret); }
  void push_write(const MemPinned_impl &m_impl, size_t size) const {
    assert(m_impl.ok());
    cl_int ret = clEnqueueWriteBuffer(_queue, m_impl.get(), CL_FALSE, 0,
				      size, m_impl.get_pointer(), 0, nullptr,
				      nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueWriteBuffer() failed. Error Code: %d", ret); }
  void push_write(const Memory_impl &m_impl, size_t size, const void *p)
    const {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueWriteBuffer(_queue, m_impl.get(), CL_FALSE, 0,
				      size, p, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueWriteBuffer() failed. Error Code: %d", ret); }
  void push_read(const MemPinned_impl &m_impl, size_t size) const {
    assert(m_impl.ok());
    cl_int ret = clEnqueueReadBuffer(_queue, m_impl.get(), CL_FALSE, 0, size,
				     m_impl.get_pointer(), 0, nullptr,
				     nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueReadBuffer() failed. Error Code: %d", ret); }
  void push_kernel(const Kernel_impl &k_impl, size_t size_global) const {
    assert(k_impl.ok());
    cl_int ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), 1U, nullptr,
					&size_global, nullptr, 0, nullptr,
					nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret); }

  void push_ndrange_kernel(const Kernel_impl &k_impl, uint dim,
			   const size_t *size_g, const size_t *size_l) const {
    assert(k_impl.ok() && 0 < dim && dim < 4 && size_g && size_l);
    cl_int ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), dim, nullptr,
					size_g, size_l, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret); } };

OCL::Queue::Queue() {};
OCL::Queue::~Queue() {};
OCL::Queue::Queue(Queue_impl &&q_impl) : _impl(new Queue_impl(move(q_impl))) {}
OCL::Queue::Queue(Queue &&q) { *this = move(q); }
OCL::Queue &Queue::operator=(Queue &&q) {
  assert(q.ok()); if (this != &q) _impl = move(q._impl); return *this; }
bool OCL::Queue::ok() const { return _impl && _impl->ok(); }
void OCL::Queue::finish() const { _impl->finish(); }
void OCL::Queue::push_write(const MemPinned &m, size_t size)
  const { assert(m.ok()); _impl->push_write(m.get(), size); }
void OCL::Queue::push_write(const Memory &m, size_t size, const void *p)
  const { assert(m.ok() && p); _impl->push_write(m.get(), size, p); }
void OCL::Queue::push_read(const MemPinned &m, size_t size) const {
  assert(m.ok()); _impl->push_read(m.get(), size); }
void OCL::Queue::push_kernel(const Kernel &k, size_t size_global) const {
  assert(k.ok()); _impl->push_kernel(k.get(), size_global); }
void OCL::Queue::push_ndrange_kernel(const Kernel &k, uint dim,
				     const size_t *size_g,
				     const size_t *size_l) const {
  assert(k.ok() && 0 < dim && dim < 4 && size_g && size_l);
  _impl->push_ndrange_kernel(k.get(), dim, size_g, size_l); }

class OCL::Context_impl {
  map<string,cl_program> _mp;
  cl_device_id _id;
  cl_context _context;
public:
  Context_impl(const cl_device_id &id) : _id(id) {
    assert(id);
    cl_int ret;
    _context = clCreateContext(nullptr, 1, &_id, nullptr, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateContext() failed. Error Code: %d", ret); }
  ~Context_impl() {
    for (const auto &pair : _mp) {
      cl_program pg = pair.second;
      clReleaseProgram(pg); }
    if (_id) clReleaseContext(_context); }
  Context_impl(Context_impl &&c_impl) { *this = move(c_impl); }
  Context_impl &operator=(Context_impl &&c_impl) {
    assert(c_impl.ok());
    if (this != &c_impl) {
      _id        = c_impl._id;
      _context   = c_impl._context;
      c_impl._id = nullptr; }
    return *this; }
  Queue_impl gen_queue() const { return Queue_impl(_id, _context); }
  Program_impl gen_program(const char *code, bool flag) {
    assert(code);
    string scode(code);
    auto it = _mp.find(scode);
    if (it != _mp.end()) return Program_impl(it->second);
    
    cl_int ret;
    cl_program pg = clCreateProgramWithSource(_context, 1, &code, nullptr,
					      &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateProgramWithSource() failed. Error Code: %d", ret);

    if (clBuildProgram(pg, 0, nullptr, (flag ? options_aggressive : options),
		       nullptr, nullptr) == CL_SUCCESS) {
      _mp[scode] = pg;
      return Program_impl(pg); }
    
    size_t size;
    ret = clGetProgramBuildInfo(pg, _id, CL_PROGRAM_BUILD_LOG, 0, nullptr,
				&size);
    if (ret != CL_SUCCESS) {
      clReleaseProgram(pg);
      throw ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret); }

    unique_ptr<char []> log(new char [size]);
    ret = clGetProgramBuildInfo(pg, _id, CL_PROGRAM_BUILD_LOG, size, log.get(),
				nullptr);
    if (ret != CL_SUCCESS) {
      clReleaseProgram(pg);
      throw ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret); }

    clReleaseProgram(pg);
    throw ERR_INT("OpenCL Build Error\n%s", log.get()); }

  MemPinned_impl gen_mem_pinned(const cl_mem_flags &mem_flags,
				const cl_map_flags &map_flags, size_t size)
    const {
    return MemPinned_impl(_context, _id, mem_flags, map_flags, size); }
  Memory_impl gen_memory(const cl_mem_flags &flags, size_t size) const {
    return Memory_impl(_context, flags, size); }
  bool ok() const { return (_id && _context); } };

OCL::Context::Context() {};
OCL::Context::~Context() {};
OCL::Context::Context(Context_impl &&c_impl)
  : _impl(new Context_impl(move(c_impl))) {}
OCL::Context::Context(Context &&c) { *this = move(c); }
OCL::Context &Context::operator=(Context &&c) {
  assert(c.ok()); if (this != &c) _impl = move(c._impl); return *this; }
bool OCL::Context::ok() const { return _impl && _impl->ok(); }
Queue OCL::Context::gen_queue() const { return Queue(_impl->gen_queue()); }
Program OCL::Context::gen_program(const char *code, bool flag) const {
  assert(code); return Program(_impl->gen_program(code, flag)); }
Program OCL::Context::gen_program(const std::string &code, bool flag) const {
  return Program(_impl->gen_program(code.c_str(), flag)); }
MemPinned OCL::Context::gen_mem_pin_hw_dr(size_t size) const {
  return MemPinned(_impl->gen_mem_pinned(CL_MEM_READ_ONLY, CL_MAP_WRITE,
					 size)); }
MemPinned OCL::Context::gen_mem_pin_hr_dw(size_t size) const {
  return MemPinned(_impl->gen_mem_pinned(CL_MEM_WRITE_ONLY, CL_MAP_READ,
					 size)); }
Memory OCL::Context::gen_mem_hw_dr(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
				  size)); }
Memory OCL::Context::gen_mem_hr_dw(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
				  size)); }
Memory OCL::Context::gen_mem_drw(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_NO_ACCESS | CL_MEM_READ_WRITE,
				  size)); }

class OCL::Device_impl {
  cl_device_id _id;
public:
  explicit Device_impl(const cl_device_id &id) : _id(id) { assert(id); }
  Context_impl gen_context() const { return Context_impl(_id); }
  bool ok() const { return _id; }
  string gen_info_string(const cl_device_info &name) {
    size_t size;
    cl_int ret = clGetDeviceInfo(_id, name, 0, nullptr, &size);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret);
    unique_ptr<char []> value(new char [size]);
    ret = clGetDeviceInfo(_id, name, size, value.get(), nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret);
    return string(value.get()); }
  template <typename T> T gen_info(const cl_device_info &name) const {
    T value;
    cl_int ret = clGetDeviceInfo(_id, name, sizeof(T), &value, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret);
    return value; } };

class OCL::Platform_impl {
  cl_platform_id _id;
public:
  explicit Platform_impl(const cl_platform_id &id) : _id(id) { assert(id); }
  bool ok() const { return _id; }
  vector<Device> gen_devices_all() const {
    cl_uint num_device;
    cl_int ret;
    ret = clGetDeviceIDs(_id, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_device);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetDeviceIDs() failed. Error Code: %d", ret);

    unique_ptr<cl_device_id []> devices(new cl_device_id [num_device]);
    ret = clGetDeviceIDs(_id, CL_DEVICE_TYPE_ALL, num_device, devices.get(),
			 nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetDeviceIDs() failed. Error Code: %d", ret);

    vector<Device> vec;
    for (cl_uint ud = 0; ud < num_device; ++ud)
      vec.emplace_back(Device_impl(devices[ud]));
    return vec; }
  string get_info(const cl_platform_info &name) {
    size_t size;
    cl_int ret = clGetPlatformInfo(_id, name, 0, nullptr, &size);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetPlatformInfo() failed. Error Code: %d", ret);
    
    unique_ptr<char []> value(new char [size]);
    ret = clGetPlatformInfo(_id, name, size, value.get(), nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clGetPlatformInfo() failed. Error Code: %d", ret);
    return string(value.get()); } };

OCL::Device::Device() {}
OCL::Device::~Device() {}
OCL::Device::Device(Device_impl &&d_impl)
  : _impl(new Device_impl(move(d_impl))) {}
OCL::Device::Device(Device &&d) { *this = move(d); }
Device &OCL::Device::operator=(Device &&d) {
  assert(d.ok()); if (this != &d) _impl = move(d._impl); return *this; }
Context OCL::Device::gen_context() const {
  return Context(_impl->gen_context()); }
bool OCL::Device::ok() const { return _impl && _impl->ok(); }
string OCL::Device::gen_info() const {
  stringstream ss;
  ss << "  Type:                 " << gen_type() << "\n"
     << "  Name:                 " << gen_name() << "\n"
     << "  Driver Version:       " << gen_driver_version() << "\n"
     << "  Compute Units:        " << gen_max_compute_units() << "\n"
     << "  Max Work-Group Size:  " << gen_max_work_group_size() << "\n"
     << "  Max Clock Freq (MHz): " << gen_max_clock_frequency() << "\n"
     << "  Global Mem Size:      " << gen_global_mem_size() << "\n"
     << "  Max Mem Alloc Size:   " << gen_max_mem_alloc_size() << "\n"
     << "  Local Mem Type:       " << gen_local_mem_type() << "\n"
     << "  Local Mem Size:       " << gen_local_mem_size() << "\n"
     << "  Rough Evaluation:     " << evaluation() << "\n";
  return ss.str(); }

Platform OCL::Device::gen_platform() const {
  cl_platform_id id = _impl->gen_info<cl_platform_id>(CL_DEVICE_PLATFORM);
  return Platform(Platform_impl(id)); }
string OCL::Device::gen_type() const {
  auto type = _impl->gen_info<cl_device_type>(CL_DEVICE_TYPE);
  auto func = [](string &str, const char *p) {
    if (str.size()) str += " ";
    str += p; };
  string str;
  if (type & CL_DEVICE_TYPE_CPU)         func(str, "CPU");
  if (type & CL_DEVICE_TYPE_GPU)         func(str, "GPU");
  if (type & CL_DEVICE_TYPE_ACCELERATOR) func(str, "ACCELERATOR");
  if (type & CL_DEVICE_TYPE_DEFAULT)     func(str, "DEFAULT");
  return str; }
string OCL::Device::gen_local_mem_type() const {
  auto type
    = _impl->gen_info<cl_device_local_mem_type>(CL_DEVICE_LOCAL_MEM_TYPE);
  if (type == CL_LOCAL)  return string("LOCAL");
  if (type == CL_GLOBAL) return string("GLOBAL");
  throw ERR_INT("bad local memory type");
  return string(""); }
string OCL::Device::gen_name() const {
  return _impl->gen_info_string(CL_DEVICE_NAME); }
string OCL::Device::gen_driver_version() const {
  return _impl->gen_info_string(CL_DRIVER_VERSION); }
uint OCL::Device::gen_max_compute_units() const {
  return _impl->gen_info<cl_uint>(CL_DEVICE_MAX_COMPUTE_UNITS); }
uint64_t OCL::Device::gen_global_mem_size() const {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_GLOBAL_MEM_SIZE); }
uint64_t OCL::Device::gen_local_mem_size() const {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_LOCAL_MEM_SIZE); }
uint64_t OCL::Device::gen_max_mem_alloc_size() const {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_MAX_MEM_ALLOC_SIZE); }
size_t OCL::Device::gen_max_work_group_size() const {
  return _impl->gen_info<size_t>(CL_DEVICE_MAX_WORK_GROUP_SIZE); }
uint OCL::Device::gen_max_clock_frequency() const {
  return _impl->gen_info<cl_uint>(CL_DEVICE_MAX_CLOCK_FREQUENCY); }
double OCL::Device::evaluation() const {
  if (!ok()) return -1.0;

  double value = 1.0;
  value *= static_cast<double>(gen_max_compute_units());
  value *= static_cast<double>(gen_max_clock_frequency());
  if      (gen_type() == "CPU") value *= 0.05;
  else if (gen_name().find("Intel") != std::string::npos) value *= 0.1;
  return value; }

OCL::Platform::Platform() {}
OCL::Platform::~Platform() {}
OCL::Platform::Platform(Platform_impl &&p_impl)
  : _impl(new Platform_impl(move(p_impl))) {}
OCL::Platform::Platform(Platform &&p) { *this = move(p); }
Platform &OCL::Platform::operator=(Platform &&p) {
  assert(p.ok()); if (this != &p) _impl = move(p._impl); return *this; }
bool OCL::Platform::ok() const { return (_impl && _impl->ok()); }
vector<Device> OCL::Platform::gen_devices_all() const {
  return _impl->gen_devices_all(); }
string OCL::Platform::gen_info() const {
  stringstream ss;
  ss << "Profile:    " << gen_profile() << "\n"
     << "Version:    " << gen_version() << "\n"
     << "Name:       " << gen_name() << "\n"
     << "Extensions: " << gen_extensions() << "\n";
  return ss.str(); }
string OCL::Platform::gen_profile() const {
  return _impl->get_info(CL_PLATFORM_PROFILE); }
string OCL::Platform::gen_version() const {
  return _impl->get_info(CL_PLATFORM_VERSION); }
string OCL::Platform::gen_name() const {
  return _impl->get_info(CL_PLATFORM_NAME); }
string OCL::Platform::gen_extensions() const {
  return _impl->get_info(CL_PLATFORM_EXTENSIONS); }

vector<Platform> OCL::gen_platforms() {
  static mutex m;
  lock_guard<mutex> lock(m);
  cl_uint num_platform;
  if (clGetPlatformIDs(0, nullptr, &num_platform) != CL_SUCCESS)
    throw ERR_INT("clGetPlatformIDs() failed.");

  unique_ptr<cl_platform_id []> platforms(new cl_platform_id [num_platform]);
  if (clGetPlatformIDs(num_platform, platforms.get(), nullptr) != CL_SUCCESS)
    throw ERR_INT("clGetPlatformIDs() failed.");

  vector<Platform> vec;
  for (cl_uint up = 0; up < num_platform; ++up)
    vec.emplace_back(Platform_impl(platforms[up]));
  return vec; }
#endif
