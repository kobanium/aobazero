#pragma once
#if defined(USE_CUDNN)

#include <iostream>
#include <sstream>

#include <cuda.h>
#include <cublas_v2.h>
#include <cudnn.h>


inline void
PrintCudaErrorMessage( const std::string &message )
{
  std::ostringstream oss;

  oss << message << "\n";
  oss << __FILE__ << " : " << __LINE__ << "\n";

  std::cerr << oss.str() << std::flush;
  exit(1);
}


inline void
CheckCudnnErrors( cudnnStatus_t status )
{
  if (status != CUDNN_STATUS_SUCCESS) {
    std::ostringstream oss;
    oss << "Cudnn failure\n";
    oss << "Error : " << cudnnGetErrorString(status);
  }
}


inline void
CheckCublasErrors( cublasStatus_t status )
{
  std::ostringstream oss;
  if (status != 0) {
    oss << "Cublas failure\n";
    oss << "Error : " << status;
    PrintCudaErrorMessage(oss.str());
  }
}


inline void
CheckCudaErrors( cudaError_t status )
{
  std::ostringstream oss;
  if (status != 0) {
    oss << "Cuda failure\n";
    oss << "Error : " << cudaGetErrorString(status);
    PrintCudaErrorMessage(oss.str());
  }
}


class CudaHandles {
  
public:
  cudnnHandle_t cudnn;
  cublasHandle_t cublas;

  CudaHandles() {
    CheckCudnnErrors(cudnnCreate(&cudnn));
    CheckCublasErrors(cublasCreate(&cublas));
  }

  ~CudaHandles() {
    CheckCudnnErrors(cudnnDestroy(cudnn));
    CheckCublasErrors(cublasDestroy(cublas));
  }  
};

inline void
SetDeviceID( CudaHandles &cuda_handles, const int new_device_id ) 
{
  int current_device_id;

  CheckCudaErrors(cudaGetDevice(&current_device_id));

  if (current_device_id != new_device_id) {
    std::cerr << "Device ID : " << current_device_id << " -> " << new_device_id << std::endl;
    CheckCudnnErrors(cudnnDestroy(cuda_handles.cudnn));
    CheckCublasErrors(cublasDestroy(cuda_handles.cublas));
    CheckCudaErrors(cudaSetDevice(new_device_id));
    CheckCudnnErrors(cudnnCreate(&cuda_handles.cudnn));
    CheckCublasErrors(cublasCreate(&cuda_handles.cublas));
  }
}

#endif
