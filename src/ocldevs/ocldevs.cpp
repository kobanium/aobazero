// 2019 Team AobaZero
// This source code is in the public domain.
#define CL_TARGET_OPENCL_VERSION 120
#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif
#include "err.hpp"
#include <exception>
#include <iostream>
#include <memory>
#include <tuple>
#include <cstdlib>
using std::unique_ptr;
using std::cout;
using std::cerr;
using std::endl;
using std::get;
using std::make_tuple;
using std::tuple;
using ErrAux::die;

struct PLParam { cl_platform_info name; const char *str; };
constexpr PLParam pf_params[] = { { CL_PLATFORM_PROFILE,    "Profile:    " },
				  { CL_PLATFORM_VERSION,    "Version:    " },
				  { CL_PLATFORM_NAME,       "name:       " },
				  { CL_PLATFORM_VENDOR,     "vendor:     " },
				  { CL_PLATFORM_EXTENSIONS, "extensions: " } };

int main() {
  // platform ID
  cl_uint num_platform;
  if (clGetPlatformIDs(0, nullptr, &num_platform) != CL_SUCCESS)
    die(ERR_INT("clGetPlatformIDs() failed."));
  
  unique_ptr<cl_platform_id []> platforms(new cl_platform_id [num_platform]);
  if (clGetPlatformIDs(num_platform, platforms.get(), nullptr) != CL_SUCCESS)
    die(ERR_INT("clGetPlatformIDs() failed."));

  unsigned int device_id = 0;
  for (cl_uint up = 0; up < num_platform; ++up) {
    const cl_platform_id &platform = platforms[up];

    // info for each platform
    cout << "Platform " << up << endl;
    for (const PLParam & param : pf_params) {
      size_t size;
      if (clGetPlatformInfo(platform, param.name, 0, nullptr, &size)
	  != CL_SUCCESS) die(ERR_INT("clGetPlatformInfo() failed."));

      unique_ptr<char []> param_value(new char [size + 1U]);
      if (clGetPlatformInfo(platform, param.name, size, param_value.get(),
			    nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetPlatformInfo() failed."));
      
      cout << "- " << param.str << param_value.get() << endl; }

    // device ID for each platform
    cl_uint num_device;
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_device)
	!= CL_SUCCESS) die(ERR_INT("clGetDeviceIDs() failed."));

    unique_ptr<cl_device_id []> devices(new cl_device_id [num_device]);
    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, num_device, devices.get(),
		       nullptr) != CL_SUCCESS)
      die(ERR_INT("clGetDeviceIDs() failed."));

    // device info for each platform
    for (cl_uint ud = 0; ud < num_device; ++ud) {
      const cl_device_id &device = devices[ud];
      cl_device_type type_;
      if (clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(cl_device_type),
			  &type_, nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      const char *type;
      switch (type_) {
      case CL_DEVICE_TYPE_CPU: type = "CPU"; break;
      case CL_DEVICE_TYPE_GPU: type = "GPU"; break;
      case CL_DEVICE_TYPE_ACCELERATOR: type = "Accelerator"; break;
      default: type = "Unknown"; break; }
      
      cl_uint vendor_id;
      if (clGetDeviceInfo(device, CL_DEVICE_VENDOR_ID, sizeof(cl_uint),
			  &vendor_id, nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      
      cl_uint compute_units;
      if (clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint),
			  &compute_units, nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      
      cl_ulong global_mem_size;
      if (clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong),
			  &global_mem_size, nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      
      size_t size;
      if (clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &size)
	  != CL_SUCCESS) die(ERR_INT("clGetDeviceInfo() failed."));
      
      unique_ptr<char []> vendor(new char [size + 1U]);
      if (clGetDeviceInfo(device, CL_DEVICE_VENDOR, size, vendor.get(),
			  nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      
      if (clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &size)
	  != CL_SUCCESS) die(ERR_INT("clGetDeviceInfo() failed."));
      unique_ptr<char []> name(new char [size + 1U]);
      if (clGetDeviceInfo(device, CL_DEVICE_NAME, size, name.get(),
			  nullptr) != CL_SUCCESS)
	die(ERR_INT("clGetDeviceInfo() failed."));
      
      cout << "- Device ID: " << device_id++ << endl;
      cout << "    Type:            " << type << endl;
      cout << "    Vendor ID:       " << vendor_id << endl;
      cout << "    Vendor:          " << vendor.get() << endl;
      cout << "    Name:            " << name.get() << endl;
      cout << "    Compute Units:   " << compute_units << endl;
      cout << "    Global Mem Size: " << global_mem_size << endl;
    } }
    
    return 0; }
