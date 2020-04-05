#pragma onece

#if defined(USE_CUDNN)

#if defined(USE_FP16)

#include <cuda_fp16.h>

typedef __half cuda_float_t;

#define CUDNN_DATA_TYPE CUDNN_DATA_HALF
#define CUDA_DATA_TYPE CUDA_R_16F

const cuda_float_t GPU_DATA_ZERO = __float2half(0.0f);
const cuda_float_t GPU_DATA_ONE = __float2half(1.0f);

inline float
ConvertToHostData( const cuda_float_t datum )
{
  return __half2float(datum);
}

inline cuda_float_t
ConvertToGPUData( const float datum )
{
  return __float2half(datum);
}

#else

typedef float cuda_float_t;

#define CUDNN_DATA_TYPE CUDNN_DATA_FLOAT
#define CUDA_DATA_TYPE CUDA_R_32F

constexpr cuda_float_t GPU_DATA_ZERO = 0.0f;
constexpr cuda_float_t GPU_DATA_ONE = 1.0f;

inline float
ConvertToHostData( const cuda_float_t datum )
{
  return datum;
}

inline cuda_float_t
ConvertToGPUData( const float datum )
{
  return datum;
}

#endif

#endif
