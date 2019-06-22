#if defined(USE_OPENCL)
#include "err.hpp"
#include "opencl.hpp"
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <cstdint>
#define CL_TARGET_OPENCL_VERSION 110
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif
using uint = unsigned int;
using std::move;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace OCL;
using namespace ErrAux;

constexpr char options[] = "-Werror -cl-std=CL1.1";

// cl_event
class OCL::Event_impl {
  cl_event _event;
public:
  explicit Event_impl() noexcept : _event(nullptr) {}
  ~Event_impl() noexcept { if (_event) clReleaseEvent(_event); }
  Event_impl(Event_impl &&e_impl) noexcept : _event(e_impl._event) {
    e_impl._event = nullptr; }
  Event_impl &operator=(Event_impl &&e_impl) noexcept {
    _event = e_impl._event;
    e_impl._event = nullptr;
    return *this; }
  void wait() const noexcept {
    cl_int ret = clWaitForEvents(1, &_event);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clwaitForEvents() failed. Error Code: %d", ret)); }
  cl_event *get_p() noexcept { return &_event; }
  const cl_event *get_p() const noexcept { return &_event; }
};
  
OCL::Events::Events() noexcept : _num(0) {}
OCL::Events::~Events() noexcept {}
OCL::Events::Events(uint num) noexcept : _num(num),
					 _impl(new Event_impl [num]) {}
OCL::Events::Events(Events &&events) noexcept : _num(events._num) {
  _impl.swap(events._impl); }
Events &OCL::Events::operator=(Events &&events) noexcept {
  _num = events._num;
  _impl.swap(events._impl);
  return *this; }
Event_impl &OCL::Events::get(uint u) noexcept {
  assert(u < _num); return _impl[u]; }
const Event_impl &OCL::Events::get(uint u) const noexcept {
  assert(u < _num); return _impl[u]; }
void OCL::Events::wait(uint u) const noexcept {
  assert(u < _num); return _impl[u].wait(); }

// cl_mem
class OCL::Memory_impl {
  cl_mem _mem;
public:
  explicit Memory_impl(const cl_context &context, const cl_mem_flags &flags,
		       size_t size) noexcept {
    assert(context);
    cl_int ret;
    _mem = clCreateBuffer(context, flags, size, nullptr, &ret);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clCreateBuffer() failed. Error Code: %d", ret)); }
  Memory_impl(Memory_impl &&k) noexcept : _mem(k._mem) {
    assert(k.ok());
    k._mem = nullptr; }
  ~Memory_impl() noexcept { if (_mem) clReleaseMemObject(_mem); }
  const cl_mem *get_p() const noexcept { return &_mem; }
  cl_mem get() const noexcept { return _mem; }
  bool ok() const noexcept { return _mem != nullptr; }
};

OCL::Memory::Memory() noexcept {}
OCL::Memory::~Memory() noexcept {}
OCL::Memory::Memory(Memory_impl &&m_impl) noexcept
  : _impl(new Memory_impl(move(m_impl))) {}
OCL::Memory::Memory(Memory &&m) noexcept : _impl(move(m._impl)) {}
Memory &OCL::Memory::operator=(Memory &&m) noexcept {
  assert(m.ok()); _impl = move(m._impl); return *this; }
bool OCL::Memory::ok() const noexcept { return _impl && _impl->ok(); }
const Memory_impl &OCL::Memory::get() const noexcept { return *_impl; }

// cl_kernel
class OCL::Kernel_impl {
  cl_kernel _kernel;
  cl_device_id _device;
public:
  explicit Kernel_impl(const cl_program &program, const cl_device_id &device,
		       const char *name) noexcept : _device(device) {
    assert(program && device && name);
    cl_int ret;
    _kernel = clCreateKernel(program, name, &ret);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clCreateKernel() failed. Error Code %d", ret)); }
  Kernel_impl(Kernel_impl &&k) noexcept : _kernel(k._kernel),
					  _device(k._device) {
    assert(k.ok());
    k._kernel = nullptr;
    k._device = nullptr; }
  ~Kernel_impl() noexcept { if (_kernel) clReleaseKernel(_kernel); }
  cl_kernel get() const noexcept { return _kernel; }
  void set_arg(uint index, size_t size, const void *value) const noexcept {
    cl_int ret = clSetKernelArg(_kernel, index, size, value);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clSetKernelArg() failed. Error Code: %d", ret)); }
  void set_arg(uint index, const Memory &m) const noexcept {
    set_arg(index, sizeof(cl_mem), m.get().get_p()); }
  bool ok() const noexcept { return (_kernel && _device); }
  template <typename T>
  T gen_info(const cl_kernel_work_group_info &name) const noexcept {
    T value;
    cl_int ret = clGetKernelWorkGroupInfo(_kernel, nullptr, name, sizeof(T),
					  &value, nullptr);
    if (ret != CL_SUCCESS) die(ERR_INT("clGetKernelWorkGroupInfo() failed."));
    return value; }
};

OCL::Kernel::Kernel() noexcept {}
OCL::Kernel::~Kernel() noexcept {}
OCL::Kernel::Kernel(Kernel_impl &&k_impl) noexcept
  : _impl(new Kernel_impl(move(k_impl))) {}
OCL::Kernel::Kernel(Kernel &&k) noexcept :  _impl(move(k._impl)) {}
const Kernel_impl &OCL::Kernel::get() const noexcept { return *_impl; }
Kernel &OCL::Kernel::operator=(Kernel &&k) noexcept {
  assert(k.ok()); _impl = move(k._impl); return *this; }
bool OCL::Kernel::ok() const noexcept { return _impl && _impl->ok(); }
void OCL::Kernel::set_arg(uint index, size_t size, const void *value)
  const noexcept {
  return _impl->set_arg(index, size, value); }
void OCL::Kernel::set_arg(uint index, const Memory &m)
  const noexcept { return _impl->set_arg(index, m); }
string OCL::Kernel::gen_info() const noexcept {
  stringstream ss;
  ss << "    Max Work-Group Size: " << gen_work_group_size()   << "\n"
     << "    Local Mem Size:      " << gen_local_mem_size()    << "\n"
     << "    Preferred WGS multi: " << gen_pref_wgs_multiple() << "\n"
     << "    private mem size:    " << gen_private_mem_size()  << "\n";
  return ss.str(); }
size_t OCL::Kernel::gen_work_group_size() const noexcept {
  return _impl->gen_info<size_t>(CL_KERNEL_WORK_GROUP_SIZE); }
cl_ulong OCL::Kernel::gen_local_mem_size() const noexcept {
  return _impl->gen_info<cl_ulong>(CL_KERNEL_LOCAL_MEM_SIZE); }
size_t OCL::Kernel::gen_pref_wgs_multiple() const noexcept {
  return
    _impl->gen_info<size_t>(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE); }
cl_ulong OCL::Kernel::gen_private_mem_size() const noexcept {
  return _impl->gen_info<cl_ulong>(CL_KERNEL_PRIVATE_MEM_SIZE); }

#include <iostream>
class OCL::Device_impl {
  cl_device_id _id;
  cl_context _context;
  cl_command_queue _queue;
  cl_program _program;
  
public:
  explicit Device_impl(const cl_device_id &id) noexcept : _id(id),
							  _program(nullptr) {
    assert(id);
    cl_int ret;
    _context = clCreateContext(nullptr, 1, &id, nullptr, nullptr, &ret);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clCreateContext() failed. Error Code: %d", ret));
    
    _queue = clCreateCommandQueue(_context, id, 0, &ret);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clCreateContext() failed. Error Code: %d", ret)); }

  ~Device_impl() noexcept {
    if (_context) clReleaseContext(_context);
    if (_queue)   clReleaseCommandQueue(_queue);
    if (_program) clReleaseProgram(_program); }

  Device_impl(Device_impl &&d_impl) noexcept :
    _id(d_impl._id), _context(d_impl._context), _queue(d_impl._queue),
    _program(d_impl._program) {
    assert(d_impl.ok());
    d_impl._id      = nullptr;
    d_impl._context = nullptr;
    d_impl._queue   = nullptr;
    d_impl._program = nullptr; }
  
  void build_program(const char *code) noexcept {
    assert(code);
    cl_int ret;
    if (_program) clReleaseProgram(_program);
    _program = clCreateProgramWithSource(_context, 1, &code, nullptr, &ret);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clCreateProgramWithSource() failed. Error Code: %d", ret));
    
    ret = clBuildProgram(_program, 0, nullptr, options, nullptr, nullptr);
    if (ret == CL_SUCCESS) return;
  
    size_t size;
    ret = clGetProgramBuildInfo(_program, _id, CL_PROGRAM_BUILD_LOG, 0,
				nullptr, &size);
    if (ret != CL_SUCCESS)
      die(ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret));

    unique_ptr<char []> log(new char [size]);
    ret = clGetProgramBuildInfo(_program, _id, CL_PROGRAM_BUILD_LOG, size,
				log.get(), nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret));
    die(ERR_INT("OpenCL Build Error\n%s", log.get())); }

  bool ok() const noexcept { return (_id && _context && _queue); }
  Memory_impl gen_memory(cl_mem_flags flags, size_t size) const noexcept {
    return Memory_impl(_context, flags, size); }
  Kernel gen_kernel(const char *name) const noexcept {
    assert(name); return Kernel_impl(_program, _id, name); }
  void finish() const noexcept {
    cl_int ret = clFinish(_queue);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clFinish() failure. Error Code: %d", ret)); }
  void push_barrier() const noexcept {
    cl_int ret = clEnqueueBarrier(_queue);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clEnqueueBarrier() failed. Error Code: %d", ret)); }
  void push_write(const Memory_impl &m_impl, size_t size, const float *p)
    const noexcept {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueWriteBuffer(_queue, m_impl.get(), CL_FALSE, 0,
				      size, p, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clEnqueueWriteBuffer() failed. Error Code: %d", ret)); }

  void push_read(const Memory_impl &m_impl, size_t size, float *p)
    const noexcept {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueReadBuffer(_queue, m_impl.get(), CL_FALSE, 0, size,
				     p, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clEnqueueReadBuffer() failed. Error Code: %d", ret)); }

  void push_kernel(const Kernel_impl &k_impl, size_t size_global)
    const noexcept {
    assert(k_impl.ok() && 0 < size_global);
    cl_int ret;
    size_t size_local
      = k_impl.gen_info<size_t>(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE);
    size_t off_g = 0;
    while (true) {
      size_t size_g = (size_global / size_local) * size_local;
      if (size_g) {
	size_global -= size_g;
	ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), 1U, &off_g, &size_g,
				     &size_local, 0, nullptr, nullptr);
	if (ret != CL_SUCCESS)
	  die(ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret));
	if (size_global == 0) return;
	off_g += size_g; }
      size_local /= 2U; } }
  
  void enqueue_kernel(const Kernel_impl &k_impl, uint dim, size_t *size_g,
		      size_t *size_l) const noexcept {
    assert(k_impl.ok() && 0 < dim && dim < 4 && size_g && size_l);
    cl_int ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), dim, nullptr,
					size_g, size_l, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret)); }
  
  string gen_info_string(const cl_device_info &name) noexcept {
    size_t size;
    cl_int ret = clGetDeviceInfo(_id, name, 0, nullptr, &size);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret));

    unique_ptr<char []> value(new char [size]);
    ret = clGetDeviceInfo(_id, name, size, value.get(), nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret));

    return string(value.get()); }

  template <typename T> T gen_info(const cl_device_info &name) const noexcept {
    T value;
    cl_int ret = clGetDeviceInfo(_id, name, sizeof(T), &value, nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetDeviceInfo() failed. Error Code: %d", ret));
    return value; }
};

class OCL::Platform_impl {
  cl_platform_id _id;
public:
  explicit Platform_impl(const cl_platform_id &id) noexcept : _id(id) {
    assert(id); }
  Platform_impl(Platform_impl &&p_impl) noexcept : _id(p_impl._id) {
    assert(p_impl.ok());
    p_impl._id = nullptr; }
  bool ok() const noexcept { return _id; }
  vector<Device> gen_devices_all() const noexcept {
    cl_uint num_device;
    cl_int ret;
    ret = clGetDeviceIDs(_id, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_device);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetDeviceIDs() failed. Error Code: %d", ret));

    unique_ptr<cl_device_id []> devices(new cl_device_id [num_device]);
    ret = clGetDeviceIDs(_id, CL_DEVICE_TYPE_ALL, num_device, devices.get(),
			 nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetDeviceIDs() failed. Error Code: %d", ret));

    vector<Device> vec;
    for (cl_uint ud = 0; ud < num_device; ++ud)
      vec.emplace_back(Device_impl(devices[ud]));
    return vec; }

  string get_info(const cl_platform_info &name) noexcept {
    size_t size;
    cl_int ret = clGetPlatformInfo(_id, name, 0, nullptr, &size);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetPlatformInfo() failed. Error Code: %d", ret));
    
    unique_ptr<char []> value(new char [size]);
    ret = clGetPlatformInfo(_id, name, size, value.get(), nullptr);
    if (ret != CL_SUCCESS)
      die(ERR_INT("clGetPlatformInfo() failed. Error Code: %d", ret));
    
    return string(value.get()); }
};

OCL::Platform::Platform() noexcept {}
OCL::Platform::~Platform() noexcept {}
OCL::Platform::Platform(Platform_impl &&p_impl) noexcept
  : _impl(new Platform_impl(move(p_impl))) {}
OCL::Platform::Platform(Platform &&p) noexcept : _impl(move(p._impl)) {}
vector<Device> OCL::Platform::gen_devices_all() const noexcept {
  return _impl->gen_devices_all(); }
string OCL::Platform::gen_info() const noexcept {
  stringstream ss;
  ss << "Profile:    " << gen_profile() << "\n"
     << "Version:    " << gen_version() << "\n"
     << "Name:       " << gen_name() << "\n"
     << "Extensions: " << gen_extensions() << "\n";
  return ss.str(); }
string OCL::Platform::gen_profile() const noexcept {
  return _impl->get_info(CL_PLATFORM_PROFILE); }
string OCL::Platform::gen_version() const noexcept {
  return _impl->get_info(CL_PLATFORM_VERSION); }
string OCL::Platform::gen_name() const noexcept {
  return _impl->get_info(CL_PLATFORM_NAME); }
string OCL::Platform::gen_extensions() const noexcept {
  return _impl->get_info(CL_PLATFORM_EXTENSIONS); }

vector<Platform> OCL::gen_platforms() noexcept {
  cl_uint num_platform;
  if (clGetPlatformIDs(0, nullptr, &num_platform) != CL_SUCCESS)
    die(ERR_INT("clGetPlatformIDs() failed."));

  unique_ptr<cl_platform_id []> platforms(new cl_platform_id [num_platform]);
  if (clGetPlatformIDs(num_platform, platforms.get(), nullptr) != CL_SUCCESS)
    die(ERR_INT("clGetPlatformIDs() failed."));

  vector<Platform> vec;
  for (cl_uint up = 0; up < num_platform; ++up)
    vec.emplace_back(Platform_impl(platforms[up]));
  return vec; }

OCL::Device::Device() noexcept {}
OCL::Device::~Device() noexcept {}
OCL::Device::Device(Device &&d) noexcept : _impl(move(d._impl)) {}
OCL::Device::Device(Device_impl &&d_impl) noexcept
  : _impl(new Device_impl(move(d_impl))) {}
Device &OCL::Device::operator=(Device &&d) noexcept {
  assert(d.ok());
  _impl = move(d._impl);
  return *this; }
void OCL::Device::build_program(const char *code) noexcept {
  return _impl->build_program(code); }
Kernel OCL::Device::gen_kernel(const char *name) const noexcept {
  return Kernel(_impl->gen_kernel(name)); }
void OCL::Device::finish() const noexcept { return _impl->finish(); }
void OCL::Device::push_barrier() const noexcept { _impl->push_barrier(); }
void OCL::Device::push_write(const Memory &m, size_t size, const float *p)
  const noexcept {
  assert(m.ok()); return _impl->push_write(m.get(), size, p); }
void OCL::Device::push_read(const Memory &m, size_t size, float *p)
  const noexcept { assert(m.ok()); _impl->push_read(m.get(), size, p); }
void OCL::Device::push_kernel(const Kernel &k, size_t size_global)
  const noexcept { assert(k.ok()); _impl->push_kernel(k.get(), size_global); }
void OCL::Device::enqueue_kernel(const Kernel &k, uint dim,
				 size_t *size_g, size_t *size_l)
  const noexcept {
  assert(k.ok()); _impl->enqueue_kernel(k.get(), dim, size_g, size_l); }
Memory OCL::Device::gen_mem_r(size_t size) const noexcept {
  return Memory(_impl->gen_memory(CL_MEM_READ_ONLY, size)); }
Memory OCL::Device::gen_mem_w(size_t size) const noexcept {
  return Memory(_impl->gen_memory(CL_MEM_WRITE_ONLY, size)); }
Memory OCL::Device::gen_mem_rw(size_t size) const noexcept {
  return Memory(_impl->gen_memory(CL_MEM_READ_WRITE, size)); }
bool OCL::Device::ok() const noexcept { return _impl && _impl->ok(); }
string OCL::Device::gen_info() const noexcept {
  stringstream ss;
  ss << "  Type:                 " << gen_type() << "\n"
     << "  Name:                 " << gen_name() << "\n"
     << "  Driver Version:       " << gen_driver_version() << "\n"
     << "  Compute Units:        " << gen_max_compute_units() << "\n"
     << "  Max Work-Group Size:  " << gen_max_work_group_size() << "\n"
     << "  Max Clock Freq (MHz): " << gen_max_clock_frequency() << "\n"
     << "  Global Mem Size:      " << gen_global_mem_size() << "\n"
     << "  Max Mem Alloc Size:   " << gen_max_mem_alloc_size() << "\n"
     << "  Local Mem type:       " << gen_local_mem_type() << "\n"
     << "  Local Mem Size:       " << gen_local_mem_size() << "\n";
  return ss.str(); }
Platform OCL::Device::gen_platform() const noexcept {
  cl_platform_id id = _impl->gen_info<cl_platform_id>(CL_DEVICE_PLATFORM);
  return Platform(Platform_impl(id)); }
string OCL::Device::gen_type() const noexcept {
  auto type = _impl->gen_info<cl_device_type>(CL_DEVICE_TYPE);
  auto func = [](string &str, const char *p){
    if (str.size()) str += " ";
    str += p; };
  string str;
  if (type & CL_DEVICE_TYPE_CPU)         func(str, "CPU");
  if (type & CL_DEVICE_TYPE_GPU)         func(str, "GPU");
  if (type & CL_DEVICE_TYPE_ACCELERATOR) func(str, "ACCELERATOR");
  if (type & CL_DEVICE_TYPE_DEFAULT)     func(str, "DEFAULT");
  return str; }
string OCL::Device::gen_local_mem_type() const noexcept {
  auto type
    = _impl->gen_info<cl_device_local_mem_type>(CL_DEVICE_LOCAL_MEM_TYPE);
  if (type == CL_LOCAL)  return string("LOCAL");
  if (type == CL_GLOBAL) return string("GLOBAL");
  die(ERR_INT("bad local memory type"));
  return string(""); }

string OCL::Device::gen_name() const noexcept {
  return _impl->gen_info_string(CL_DEVICE_NAME); }
string OCL::Device::gen_driver_version() const noexcept {
  return _impl->gen_info_string(CL_DRIVER_VERSION); }
uint OCL::Device::gen_max_compute_units() const noexcept {
  return _impl->gen_info<cl_uint>(CL_DEVICE_MAX_COMPUTE_UNITS); }
uint64_t OCL::Device::gen_global_mem_size() const noexcept {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_GLOBAL_MEM_SIZE); }
uint64_t OCL::Device::gen_local_mem_size() const noexcept {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_LOCAL_MEM_SIZE); }
uint64_t OCL::Device::gen_max_mem_alloc_size() const noexcept {
  return _impl->gen_info<cl_ulong>(CL_DEVICE_MAX_MEM_ALLOC_SIZE); }
size_t OCL::Device::gen_max_work_group_size() const noexcept {
  return _impl->gen_info<size_t>(CL_DEVICE_MAX_WORK_GROUP_SIZE); }
uint OCL::Device::gen_max_clock_frequency() const noexcept {
  return _impl->gen_info<cl_uint>(CL_DEVICE_MAX_CLOCK_FREQUENCY); }
#endif
