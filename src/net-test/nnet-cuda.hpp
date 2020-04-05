#pragma once
#if defined(USE_CUDNN)

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cuda_layers.hpp"
#include "cuda_wrapper.hpp"
#include "nnet.hpp"


class NNetCUDA {
private:
  using uint = unsigned int;
  using ushort = unsigned short;
  using row_t = std::unique_ptr<float []>;

  CudaHandles cuda_handles;
  cudnnTensorDescriptor_t input_descriptor;
  cudnnTensorDescriptor_t hidden_descriptor;
  cudnnTensorDescriptor_t conv_value_descriptor;
  cudnnTensorDescriptor_t value_ip_descriptor;
  cudnnTensorDescriptor_t conv_policy_descriptor;
  cudnnTensorDescriptor_t policy_descriptor;
  cudnnTensorDescriptor_t value_descriptor;

  cuda_float_t *input_ptr;
  cuda_float_t *h1_ptr;
  cuda_float_t *h2_ptr;
  cuda_float_t *res_ptr;
  cuda_float_t *conv_value_ptr;
  cuda_float_t *value_ip_ptr;
  cuda_float_t *value_ptr;
  cuda_float_t *conv_policy_ptr;
  cuda_float_t *policy_ptr;

  cuda_float_t *input_data;
  cuda_float_t *policy;
  cuda_float_t *value;
  
  std::unique_ptr<Add> add = nullptr;
  std::unique_ptr<ReLU> relu = nullptr;
  std::unique_ptr<Softmax> softmax = nullptr;
  std::unique_ptr<Tanh> tanh = nullptr;

  // First Conv Layer
  std::unique_ptr<Convolution> conv = nullptr;
  std::unique_ptr<Bias> bias_conv = nullptr;
  std::unique_ptr<BatchNormalization> bn = nullptr;

  // Last BatchNorm Layer

  // Policy Head
  std::unique_ptr<Convolution> conv_policy1 = nullptr;
  std::unique_ptr<Bias> bias_conv_policy1 = nullptr;
  std::unique_ptr<BatchNormalization> bn_policy = nullptr;
  std::unique_ptr<Convolution> conv_policy2 = nullptr;
  std::unique_ptr<Bias> bias_conv_policy2 = nullptr;

  // Value Head
  std::unique_ptr<Convolution> conv_value = nullptr;
  std::unique_ptr<Bias> bias_conv_value = nullptr;
  std::unique_ptr<BatchNormalization> bn_value = nullptr;
  std::unique_ptr<FullyConnected> value_ip1 = nullptr;
  std::unique_ptr<Bias> bias_value_ip1 = nullptr;
  std::unique_ptr<FullyConnected> value_ip2 = nullptr;
  std::unique_ptr<Bias> bias_value_ip2 = nullptr;


  // Residual Blocks
  std::unique_ptr<ResBlock> res_blocks[19];

  // Maximum batch size
  uint max_batch_size;

public:

  NNetCUDA(){};
  ~NNetCUDA();
  void reset(uint maxsize_batch,
	     const std::vector<std::pair<uint, row_t> > &wght,
	     int device_id, bool use_half = true) noexcept;

  void ff(uint size_batch, const float *input, const uint *sizes_nnmove,
	  const ushort *nnmoves, float *probs, float *values) noexcept;
};


#endif
