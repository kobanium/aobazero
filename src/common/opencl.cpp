#if defined(USE_OPENCL_AOBA)
#include "err.hpp"
#include "opencl.hpp"
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
using std::move;
using std::mutex;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace OCL;
using namespace ErrAux;

constexpr char options[] = "-Werror -cl-std=CL1.2";

// cl_event
class OCL::Event_impl {
  cl_event _event;

public:
  explicit Event_impl() : _event(nullptr) {}
  ~Event_impl() { if (_event) release(); }
  Event_impl(Event_impl &&e_impl) : _event(e_impl._event) {
    e_impl._event = nullptr; }
  Event_impl &operator=(Event_impl &&e_impl) {
    if (_event) release();
    _event = e_impl._event;
    e_impl._event = nullptr;
    return *this; }
  void release() {
    cl_int ret = clReleaseEvent(_event);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clReleaseEvent() failed. Error Code: %d", ret);
    _event = nullptr; }
  void wait() {
    cl_int ret = clWaitForEvents(1, &_event);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clwaitForEvents() failed. Error Code: %d", ret);
    release(); }
  cl_event *get() { return &_event; }
  bool ok() const { return _event != nullptr; }
  const cl_event *get() const { return &_event; }
};
  
OCL::Event::Event() : _impl(new Event_impl) {}
OCL::Event::~Event() {}
OCL::Event::Event(Event &&e) : _impl(move(e._impl)) {}
Event &OCL::Event::operator=(Event &&e) {
  assert(e.ok()); _impl = move(e._impl); return *this; }
Event_impl &OCL::Event::get() { return *_impl; }
void OCL::Event::wait() const { return _impl->wait(); }
bool OCL::Event::ok() const { return _impl && _impl->ok(); }
const Event_impl &OCL::Event::get() const { return *_impl; }

// cl_mem
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
  Memory_impl(Memory_impl &&k) : _mem(k._mem) { k._mem = nullptr; }
  ~Memory_impl() { if (_mem) clReleaseMemObject(_mem); }
  const cl_mem *get_p() const { return &_mem; }
  cl_mem get() const { return _mem; }
  bool ok() const { return _mem != nullptr; }
};

OCL::Memory::Memory() {}
OCL::Memory::~Memory() {}
OCL::Memory::Memory(Memory_impl &&m_impl)
  : _impl(new Memory_impl(move(m_impl))) {}
OCL::Memory::Memory(Memory &&m) : _impl(move(m._impl)) {}
Memory &OCL::Memory::operator=(Memory &&m) {
  assert(m.ok()); _impl = move(m._impl); return *this; }
void OCL::Memory::clear() { _impl.reset(nullptr); }
bool OCL::Memory::ok() const { return _impl && _impl->ok(); }
const Memory_impl &OCL::Memory::get() const { return *_impl; }

// cl_kernel
class OCL::Kernel_impl {
  cl_kernel _kernel;
public:
  explicit Kernel_impl(const cl_program &program, const char *name) {
    assert(program && name);
    cl_int ret;
    _kernel = clCreateKernel(program, name, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateKernel() failed. Error Code %d", ret); }
  Kernel_impl(Kernel_impl &&k) : _kernel(k._kernel) { k._kernel = nullptr; }
  ~Kernel_impl() { if (_kernel) clReleaseKernel(_kernel); }
  cl_kernel get() const { return _kernel; }
  void set_arg(uint index, size_t size, const void *value) const {
    cl_int ret = clSetKernelArg(_kernel, index, size, value);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clSetKernelArg() failed. Error Code: %d", ret); }
  void set_arg(uint index, const Memory &m) const {
    set_arg(index, sizeof(cl_mem), m.get().get_p()); }
  bool ok() const { return _kernel; }
  template <typename T>
  T gen_info(const cl_kernel_work_group_info &name) const {
    T value;
    cl_int ret = clGetKernelWorkGroupInfo(_kernel, nullptr, name, sizeof(T),
					  &value, nullptr);
    if (ret != CL_SUCCESS) throw ERR_INT("clGetKernelWorkGroupInfo() failed.");
    return value; }
};

OCL::Kernel::Kernel() {}
OCL::Kernel::~Kernel() {}
OCL::Kernel::Kernel(Kernel_impl &&k_impl)
  : _impl(new Kernel_impl(move(k_impl))) {}
OCL::Kernel::Kernel(Kernel &&k) :  _impl(move(k._impl)) {}
const Kernel_impl &OCL::Kernel::get() const { return *_impl; }
Kernel &OCL::Kernel::operator=(Kernel &&k) {
  assert(k.ok()); _impl = move(k._impl); return *this; }
bool OCL::Kernel::ok() const { return _impl && _impl->ok(); }
void OCL::Kernel::set_arg(uint index, size_t size, const void *value) const {
  return _impl->set_arg(index, size, value); }
void OCL::Kernel::set_arg(uint index, const Memory &m) const {
  return _impl->set_arg(index, m); }
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

// cl_kernel
#include <iostream>
class OCL::Program_impl {
  cl_program _pg;
public:
  explicit Program_impl(const char *code, cl_device_id dev,
			cl_context context) {
    assert(code && dev && context);
    cl_int ret;
    _pg = clCreateProgramWithSource(context, 1, &code, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateProgramWithSource() failed. Error Code: %d", ret);

    ret = clBuildProgram(_pg, 0, nullptr, options, nullptr, nullptr);
    if (ret == CL_SUCCESS) return;

    size_t size;
    ret = clGetProgramBuildInfo(_pg, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr,
				&size);
    if (ret != CL_SUCCESS)
      throw ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret);

    unique_ptr<char []> log(new char [size]);
    ret = clGetProgramBuildInfo(_pg, dev, CL_PROGRAM_BUILD_LOG, size,
				log.get(), nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("cl_GetProgramBuildInfo() failed. Error Code: %d", ret);
    clReleaseProgram(_pg);
    throw ERR_INT("OpenCL Build Error\n%s", log.get()); }

  Program_impl(Program_impl &&p_impl) : _pg(p_impl._pg) {
    p_impl._pg  = nullptr; }

  ~Program_impl() { if (_pg) clReleaseProgram(_pg); }
  bool ok() const { return _pg; }
  Kernel_impl gen_kernel(const char *name) const {
    assert(name); return Kernel_impl(_pg, name); }
};

OCL::Program::Program() {}
OCL::Program::~Program() {}
OCL::Program::Program(Program_impl &&p_impl)
  : _impl(new Program_impl(move(p_impl))) {}
OCL::Program::Program(Program &&p) : _impl(move(p._impl)) {}
Program &OCL::Program::operator=(Program &&p) {
  assert(p.ok()); _impl = move(p._impl); return *this; }
const Program_impl &OCL::Program::get() const { return *_impl; }
bool OCL::Program::ok() const { return _impl && _impl->ok(); }
Kernel OCL::Program::gen_kernel(const char *name) const {
  assert(name); return Kernel(_impl->gen_kernel(name)); }

class OCL::Queue_impl {
  cl_device_id _id;
  cl_context _context;
  cl_command_queue _queue;
public:
  Queue_impl(const cl_device_id &id) : _id(id), _context(nullptr),
				       _queue(nullptr) {
    assert(id);
    cl_int ret;
    _context = clCreateContext(nullptr, 1, &_id, nullptr, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clCreateContext() failed. Error Code: %d", ret);

    _queue = clCreateCommandQueue(_context, _id,
				  //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
				  0, &ret);
    if (ret != CL_SUCCESS) {
      if (_context) clReleaseContext(_context);
      throw ERR_INT("clCreateContext() failed. Error Code: %d", ret); } }
  ~Queue_impl() {
    if (_queue) clReleaseCommandQueue(_queue);
    if (_context) clReleaseContext(_context); }
  Queue_impl(Queue_impl &&q_impl) : _id(q_impl._id), _context(q_impl._context),
				    _queue(q_impl._queue) {
    q_impl._id      = nullptr;
    q_impl._queue   = nullptr;
    q_impl._context = nullptr; }
  Program_impl gen_program(const char *code) const {
    assert(code); return Program_impl(code, _id, _context); }
  Memory_impl gen_memory(const cl_mem_flags &flags, size_t size) const {
    return Memory_impl(_context, flags, size); }
  bool ok() const { return (_queue && _context); }
  void finish() const {
    cl_int ret = clFinish(_queue);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clFinish() failure. Error Code: %d", ret); }
  void *push_map(const Memory_impl &m_impl, const cl_map_flags &flags,
		 size_t size) const {
    assert(m_impl.ok());
    cl_int ret;
    void *p = clEnqueueMapBuffer(_queue, m_impl.get(), CL_FALSE, flags,
				 0, size, 0, nullptr, nullptr, &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueMapBuffer() failure. Error Code: %d", ret);
    return p; }
  void *push_map(const Memory_impl &m_impl, const cl_map_flags &flags,
		 size_t size, Event_impl &e_impl) const {
    assert(m_impl.ok());
    cl_int ret;
    void *p = clEnqueueMapBuffer(_queue, m_impl.get(), CL_FALSE, flags,
				 0, size, 0, nullptr, e_impl.get(), &ret);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueMapBuffer() failure. Error Code: %d", ret);
    return p; }
  void push_unmap(const Memory_impl &m_impl, void *ptr) const {
    assert(m_impl.ok() && ptr);
    cl_int ret = clEnqueueUnmapMemObject(_queue, m_impl.get(), ptr, 0, nullptr,
					 nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueUnmapObject() failure. Error Code: %d", ret); }
  void push_unmap(const Memory_impl &m_impl, void *ptr, Event_impl &e_impl)
    const {
    assert(m_impl.ok() && ptr);
    cl_int ret = clEnqueueUnmapMemObject(_queue, m_impl.get(), ptr, 0, nullptr,
					 e_impl.get());
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueUnmapObject() failure. Error Code: %d", ret); }
  void push_write(const Memory_impl &m_impl, size_t size, const void *p)
    const {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueWriteBuffer(_queue, m_impl.get(), CL_FALSE, 0,
				      size, p, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueWriteBuffer() failed. Error Code: %d", ret); }

  void push_read(const Memory_impl &m_impl, size_t size, void *p) const {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueReadBuffer(_queue, m_impl.get(), CL_FALSE, 0, size,
				     p, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueReadBuffer() failed. Error Code: %d", ret); }

  void push_read(const Memory_impl &m_impl, size_t size, void *p,
		 Event_impl &e_impl) const {
    assert(m_impl.ok() && p);
    cl_int ret = clEnqueueReadBuffer(_queue, m_impl.get(), CL_FALSE, 0, size,
				     p, 0, nullptr, e_impl.get());
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueueReadBuffer() failed. Error Code: %d", ret); }

  void push_kernel(const Kernel_impl &k_impl, size_t size_global) const {
    assert(k_impl.ok() && 0 < size_global);
    cl_int ret;
    size_t size_local
      = k_impl.gen_info<size_t>(CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE);
    size_t size_g = (size_global / size_local) * size_local;

    if (size_g) {
      size_global -= size_g;
      ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), 1U, nullptr, &size_g,
				   &size_local, 0, nullptr, nullptr);
      if (ret != CL_SUCCESS)
	throw ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret); }
    
    if (size_global == 0) return;

    ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), 1U, &size_g,
				 &size_global, &size_global, 0, nullptr,
				 nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret); }

  void push_ndrange_kernel(const Kernel_impl &k_impl, uint dim,
			   const size_t *size_g, const size_t *size_l) const {
    assert(k_impl.ok() && 0 < dim && dim < 4 && size_g && size_l);
    cl_int ret = clEnqueueNDRangeKernel(_queue, k_impl.get(), dim, nullptr,
					size_g, size_l, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS)
      throw ERR_INT("clEnqueNDRangeKernel() failed. Error Code: %d", ret); }
};

OCL::Queue::Queue() {};
OCL::Queue::~Queue() {};
OCL::Queue::Queue(Queue_impl &&q_impl) : _impl(new Queue_impl(move(q_impl))) {}
OCL::Queue::Queue(Queue &&q) : _impl(move(q._impl)) {}
OCL::Queue &Queue::operator=(Queue &&q) {
  assert(q.ok()); _impl = move(q._impl); return *this; }
void OCL::Queue::reset() { _impl.reset(nullptr); }
bool OCL::Queue::ok() const { return _impl && _impl->ok(); }

Program OCL::Queue::gen_program(const char *code) const {
  return Program(_impl->gen_program(code)); }
Memory OCL::Queue::gen_mem_map_hw_dr(size_t size) const {
  return Memory(_impl->gen_memory((CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY
				   | CL_MEM_ALLOC_HOST_PTR), size)); }
Memory OCL::Queue::gen_mem_map_hr_dw(size_t size) const {
  return Memory(_impl->gen_memory((CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY
				   | CL_MEM_ALLOC_HOST_PTR), size)); }
Memory OCL::Queue::gen_mem_hw_dr(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_WRITE_ONLY | CL_MEM_READ_ONLY,
				  size)); }
Memory OCL::Queue::gen_mem_hr_dw(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
				  size)); }
Memory OCL::Queue::gen_mem_drw(size_t size) const {
  return Memory(_impl->gen_memory(CL_MEM_HOST_NO_ACCESS | CL_MEM_READ_WRITE,
				  size)); }
void OCL::Queue::finish() const { _impl->finish(); }
void *OCL::Queue::push_map_w(const Memory &m, size_t size) const {
  assert(m.ok());
  return _impl->push_map(m.get(), CL_MAP_WRITE_INVALIDATE_REGION, size); }
void *OCL::Queue::push_map_r(const Memory &m, size_t size) const {
  assert(m.ok()); return _impl->push_map(m.get(), CL_MAP_READ, size); }
void *OCL::Queue::push_map_w(const Memory &m, size_t size, Event &e) const {
  assert(m.ok());
  return _impl->push_map(m.get(), CL_MAP_WRITE_INVALIDATE_REGION, size,
			 e.get()); }
void *OCL::Queue::push_map_r(const Memory &m, size_t size, Event &e) const {
  assert(m.ok());
  return _impl->push_map(m.get(), CL_MAP_READ, size, e.get()); }
void OCL::Queue::push_unmap(const Memory &m, void *ptr) const {
  assert(m.ok() && ptr); _impl->push_unmap(m.get(), ptr); }
void OCL::Queue::push_unmap(const Memory &m, void *ptr, Event &e) const {
  assert(m.ok() && ptr); _impl->push_unmap(m.get(), ptr, e.get()); }
void OCL::Queue::push_write(const Memory &m, size_t size, const void *p)
  const { assert(m.ok() && p); _impl->push_write(m.get(), size, p); }
void OCL::Queue::push_read(const Memory &m, size_t size, void *p) const {
  assert(m.ok() && p); _impl->push_read(m.get(), size, p); }
void OCL::Queue::push_read(const Memory &m, size_t size, void *p, Event &e)
  const {
  assert(m.ok() && p); _impl->push_read(m.get(), size, p, e.get()); }
void OCL::Queue::push_kernel(const Kernel &k, size_t size_global) const {
  assert(k.ok()); _impl->push_kernel(k.get(), size_global); }
void OCL::Queue::push_ndrange_kernel(const Kernel &k, uint dim,
				     const size_t *size_g,
				     const size_t *size_l) const {
  assert(k.ok() && 0 < dim && dim < 4 && size_g && size_l);
  _impl->push_ndrange_kernel(k.get(), dim, size_g, size_l); }

class OCL::Device_impl {
  cl_device_id _id;
public:
  explicit Device_impl(const cl_device_id &id) : _id(id) { assert(id); }
  ~Device_impl() {}
  Device_impl(Device_impl &&d_impl) : _id(d_impl._id) { assert(d_impl.ok()); }
  Queue gen_queue() const { return Queue_impl(_id); }
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
    return value; }
};

class OCL::Platform_impl {
  cl_platform_id _id;
public:
  explicit Platform_impl(const cl_platform_id &id) : _id(id) { assert(id); }
  Platform_impl(Platform_impl &&p_impl) : _id(p_impl._id) {
    assert(p_impl.ok());
    p_impl._id = nullptr; }
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
    
    return string(value.get()); }
};

OCL::Platform::Platform() {}
OCL::Platform::~Platform() {}
OCL::Platform::Platform(Platform_impl &&p_impl)
  : _impl(new Platform_impl(move(p_impl))) {}
OCL::Platform::Platform(Platform &&p) : _impl(move(p._impl)) {}
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

OCL::Device::Device() {}
OCL::Device::~Device() {}
OCL::Device::Device(Device &&d) : _impl(move(d._impl)) {}
OCL::Device::Device(Device_impl &&d_impl)
  : _impl(new Device_impl(move(d_impl))) {}
Device &OCL::Device::operator=(Device &&d) {
  assert(d.ok()); _impl = move(d._impl); return *this; }
Queue OCL::Device::gen_queue() const { return Queue(_impl->gen_queue()); }
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
     << "  Local Mem type:       " << gen_local_mem_type() << "\n"
     << "  Local Mem Size:       " << gen_local_mem_size() << "\n";
  return ss.str(); }
Platform OCL::Device::gen_platform() const {
  cl_platform_id id = _impl->gen_info<cl_platform_id>(CL_DEVICE_PLATFORM);
  return Platform(Platform_impl(id)); }
string OCL::Device::gen_type() const {
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
#endif
