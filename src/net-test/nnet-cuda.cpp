// 2019 Team AobaZero
// This source code is in the public domain
#if defined(USE_CUDNN)

#include "err.hpp"
#include "iobase.hpp"

#include "nnet-cuda.hpp"
#include "cuda_wrapper.hpp"
#include "cuda_layers.hpp"


constexpr char msg_bad_wght_dim[] = "bad weight dimension";



NNetCUDA::~NNetCUDA()
{
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(input_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(hidden_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(conv_value_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(value_ip_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(value_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(conv_policy_descriptor));
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(policy_descriptor));
    CheckCudnnErrors(cudnnDestroyOpTensorDescriptor(op_tensor_descriptor));
    
    CheckCudaErrors(cudaFree(input_ptr));
    CheckCudaErrors(cudaFree(h1_ptr));
    CheckCudaErrors(cudaFree(h2_ptr));
    CheckCudaErrors(cudaFree(res_ptr));
    CheckCudaErrors(cudaFree(conv_policy_ptr));
    CheckCudaErrors(cudaFree(policy_ptr));
    CheckCudaErrors(cudaFree(conv_value_ptr));
    CheckCudaErrors(cudaFree(value_ip_ptr));
    CheckCudaErrors(cudaFree(value_ptr));
}


void NNetCUDA::reset(const uint maxsize_batch, const std::vector<std::pair<uint, row_t> > &wght,
		     const int device_id, bool use_half) noexcept {

  constexpr uint nrow_input = 4U;
  constexpr uint nrow_head  = 14U;
  uint nrow = static_cast<uint>(wght.size());

  if (nrow < nrow_input + nrow_head) {
    ErrAux::die(ERR_INT(msg_bad_wght_dim));
  }
  uint nrow_res = nrow - nrow_input - nrow_head;
  if (nrow_res % 8U) {
    ErrAux::die(ERR_INT(msg_bad_wght_dim));
  }
  
  assert(0 < maxsize_batch);

  const uint _head_index = nrow_res + 4U;

  const uint _resnet_nout = wght[1].first;
  const uint size_kernel = 3 * 3;


  const uint _policy1_nout = wght[_head_index + 1U].first;
  const uint _value1_nout = wght[_head_index + 7U].first;
  const uint _value2_nout = wght[_head_index + 11U].first;

  
  // set gpu id
  SetDeviceID(cuda_handles, device_id);


  // Create descriptors.
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&input_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&hidden_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&conv_value_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&value_ip_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&value_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&conv_policy_descriptor));
  CheckCudnnErrors(cudnnCreateTensorDescriptor(&policy_descriptor));
  CheckCudnnErrors(cudnnCreateOpTensorDescriptor(&op_tensor_descriptor));

  // Set descriptors.

  // First conv layers
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(input_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, NNAux::nch_input, NNAux::height, NNAux::width));

  // Residual blocks
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(hidden_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, _resnet_nout, NNAux::height, NNAux::width));

  // Value 1st hidden layer (convolution)
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(conv_value_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, _value1_nout, NNAux::height, NNAux::width));
  // Value 2nd hidden layer (fully connected)
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(value_ip_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, _value2_nout, 1, 1));
  // Value output
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(value_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, 1, 1, 1));

  // Policy hidden conv layer
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(conv_policy_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, _policy1_nout, NNAux::height, NNAux::width));
  // Policy output
  CheckCudnnErrors(cudnnSetTensor4dDescriptor(policy_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, maxsize_batch, NNAux::nch_out_policy, NNAux::height, NNAux::width));


  
  // Malloc GPU memory.
  CheckCudaErrors(cudaMalloc((void**)&input_ptr, sizeof(float) * maxsize_batch * NNAux::nch_input * NNAux::size_plane));
  CheckCudaErrors(cudaMalloc((void**)&h1_ptr, sizeof(float) * maxsize_batch * _resnet_nout * NNAux::size_plane));
  CheckCudaErrors(cudaMalloc((void**)&h2_ptr, sizeof(float) * maxsize_batch * _resnet_nout * NNAux::size_plane));
  CheckCudaErrors(cudaMalloc((void**)&res_ptr, sizeof(float) * maxsize_batch * _resnet_nout * NNAux::size_plane));

  // Value 1st hidden layer (convolution)
  CheckCudaErrors(cudaMalloc((void**)&conv_value_ptr, sizeof(float) * maxsize_batch * _value1_nout * NNAux::size_plane));
  // Value 2nd hidden layer (fully connected)
  CheckCudaErrors(cudaMalloc((void**)&value_ip_ptr, sizeof(float) * maxsize_batch * _value2_nout));
  // Value output
  CheckCudaErrors(cudaMalloc((void**)&value_ptr, sizeof(float) * maxsize_batch));

  // Policy hidden conv layer
  CheckCudaErrors(cudaMalloc((void**)&conv_policy_ptr, sizeof(float) * maxsize_batch * _policy1_nout * NNAux::size_plane));
  // Policy output
  CheckCudaErrors(cudaMalloc((void**)&policy_ptr, sizeof(float) * maxsize_batch * NNAux::nch_out_policy * NNAux::size_plane));  


  // Temporary scales and biases for BatchNormalization Layer
  float bn_scale[_resnet_nout], bn_bias[_resnet_nout];

  for (uint u = 0; u < _resnet_nout; u++) {
    bn_scale[u] = 1.0;
    bn_bias[u]  = 0.0;
  }
  

  // First Layer
  float *weight_ptr = wght[0].second.get();
  float *bias_ptr = wght[1].second.get();
  float *mean_ptr = wght[2].second.get();
  float *variance_ptr = wght[3].second.get();

  // Load first layer
  conv.reset(new Convolution(cuda_handles.cudnn, input_descriptor, hidden_descriptor, _resnet_nout, NNAux::nch_input, 3, weight_ptr));
  bn.reset(new BatchNormalization(_resnet_nout, bn_scale, bn_bias, mean_ptr, variance_ptr));

  uint index = 4;
  // Load residual blocks
  for (uint u = 0; u < nrow_res / 8U; u++) {
    if (wght[index].first != _resnet_nout * _resnet_nout * size_kernel
	|| wght[index + 1U].first != _resnet_nout
	|| wght[index + 2U].first != _resnet_nout
	|| wght[index + 3U].first != _resnet_nout
	|| wght[index + 4U].first != _resnet_nout * _resnet_nout * size_kernel
	|| wght[index + 5U].first != _resnet_nout
	|| wght[index + 6U].first != _resnet_nout
	|| wght[index + 7U].first != _resnet_nout) {
      ErrAux::die(ERR_INT(msg_bad_wght_dim));

      ResParameter res_param;

      res_param.conv1_weight = wght[index].second.get();
      res_param.bn1_scale = bn_scale;
      res_param.bn1_bias = bn_bias;
      res_param.bn1_mean = wght[index + 2U].second.get();
      res_param.bn1_variance = wght[index + 3U].second.get();
      res_param.conv2_weight = wght[index + 4U].second.get();
      res_param.bn2_scale = bn_scale;
      res_param.bn2_bias = bn_bias;
      res_param.bn2_mean = wght[index + 6U].second.get();
      res_param.bn2_variance = wght[index + 7U].second.get();
      
      res_blocks[u].reset(new ResBlock(cuda_handles, hidden_descriptor, res_param, _resnet_nout));
      index += 8U;
    }
  }

  // Policy head
  conv_policy1.reset(new Convolution(cuda_handles.cudnn, hidden_descriptor, conv_policy_descriptor, _policy1_nout, _resnet_nout, 1, wght[_head_index].second.get()));
  bias_conv_policy1.reset(new Bias(cuda_handles.cudnn, _policy1_nout, wght[_head_index + 1U].second.get()));
  bn_policy.reset(new BatchNormalization(_policy1_nout, bn_scale, bn_bias, wght[_head_index + 2U].second.get(), wght[_head_index + 3U].second.get()));
  conv_policy2.reset(new Convolution(cuda_handles.cudnn, conv_policy_descriptor, policy_descriptor, NNAux::nch_out_policy, _policy1_nout, 1, wght[_head_index + 4U].second.get()));
  bias_conv_policy2.reset(new Bias(cuda_handles.cudnn, NNAux::nch_out_policy, wght[_head_index + 5U].second.get()));

  // Value head
  conv_value.reset(new Convolution(cuda_handles.cudnn, hidden_descriptor, conv_value_descriptor, _value1_nout, _resnet_nout, 1, wght[_head_index + 6U].second.get()));
  bias_conv_value.reset(new Bias(cuda_handles.cudnn, _value1_nout, wght[_head_index + 7U].second.get()));
  bn_value.reset(new BatchNormalization(_value1_nout, bn_scale, bn_bias, wght[_head_index + 8U].second.get(), wght[_head_index + 9U].second.get()));
  value_ip1.reset(new FullyConnected(_value1_nout * NNAux::size_plane, _value2_nout, wght[_head_index + 10U].second.get()));
  bias_value_ip1.reset(new Bias(cuda_handles.cudnn, _value2_nout, wght[_head_index + 11U].second.get()));
  value_ip2.reset(new FullyConnected(_value2_nout, 1, wght[_head_index + 12U].second.get()));
  bias_value_ip2.reset(new Bias(cuda_handles.cudnn, 1, wght[_head_index + 13U].second.get()));

  // activation layers
  relu.reset(new ReLU());
  softmax.reset(new Softmax());
  tanh.reset(new Tanh());
  
  // add layer
  add.reset(new Add());
  
}


void NNetCUDA::ff(uint size_batch, const float *input, const uint *sizes_nnmove,
		  const ushort *nnmoves, float *probs, float *values) noexcept {

  // Send input planes.
  CheckCudaErrors(cudaMemcpy(input_ptr, input, sizeof(float) * size_batch * NNAux::size_plane, cudaMemcpyHostToDevice));

  conv->Forward(cuda_handles.cudnn, input_descriptor, hidden_descriptor, input_ptr, h1_ptr);
  bn->Forward(cuda_handles.cudnn, hidden_descriptor, h1_ptr, h1_ptr);
  relu->Forward(cuda_handles.cudnn, hidden_descriptor, h1_ptr);

  // Residual Blocks
  for (int i = 0; i < 19; i++) {
    res_blocks[i]->Forward(cuda_handles,
			   hidden_descriptor,
			   h1_ptr,
			   h2_ptr,
			   res_ptr,
			   h1_ptr,
			   relu,
			   add,
			   size_batch);
  }

  // Policy haed
  conv_policy1->Forward(cuda_handles.cudnn, hidden_descriptor, conv_policy_descriptor, h1_ptr, conv_policy_ptr);
  bias_conv_policy1->Forward(cuda_handles.cudnn, conv_policy_descriptor, conv_policy_ptr);
  bn_policy->Forward(cuda_handles.cudnn, conv_policy_descriptor, conv_policy_ptr, conv_policy_ptr);
  relu->Forward(cuda_handles.cudnn, conv_policy_descriptor, conv_policy_ptr);
  conv_policy2->Forward(cuda_handles.cudnn, conv_policy_descriptor, policy_descriptor, conv_policy_ptr, policy_ptr);
  softmax->Forward(cuda_handles.cudnn, policy_descriptor, policy_ptr);

  // Value head
  conv_value->Forward(cuda_handles.cudnn, hidden_descriptor, conv_value_descriptor, h1_ptr, conv_value_ptr);
  bias_conv_value->Forward(cuda_handles.cudnn, conv_value_descriptor, conv_value_ptr);
  bn_value->Forward(cuda_handles.cudnn, conv_value_descriptor, conv_value_ptr, conv_value_ptr);
  value_ip1->Forward(cuda_handles.cublas, conv_value_ptr, value_ip_ptr, size_batch);
  bias_value_ip1->Forward(cuda_handles.cudnn, value_ip_descriptor, value_ip_ptr);
  value_ip2->Forward(cuda_handles.cublas, value_ip_ptr, value_ptr, size_batch);
  bias_value_ip2->Forward(cuda_handles.cudnn, value_descriptor, value_ptr);
  tanh->Forward(cuda_handles.cudnn, value_descriptor, value_ptr);

  float policy[size_batch * NNAux::nch_out_policy * NNAux::size_plane];
  
  // Get Values
  CheckCudaErrors(cudaMemcpy(value_ptr, values, sizeof(float) * size_batch, cudaMemcpyDeviceToHost));
  CheckCudaErrors(cudaMemcpy(policy_ptr, policy, sizeof(float) * size_batch * NNAux::nch_out_policy * NNAux::size_plane, cudaMemcpyDeviceToHost));
}



#endif
