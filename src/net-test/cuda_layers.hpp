#pragma once
#if defined(USE_CUDNN)

#include <memory>

#include "cuda_wrapper.hpp"


class Convolution {
private:
  int number_of_filters;
  int number_of_input_planes;
  int filter_height;
  int filter_width;
  float *filter_data_gpu = nullptr;

  size_t workspace_size = 0;
  void *workspace = nullptr;

  cudnnFilterDescriptor_t filter_descriptor;
  cudnnConvolutionDescriptor_t convolution_descriptor;  
  cudnnConvolutionFwdAlgo_t forward_algorithm;

public:
  Convolution( cudnnHandle_t &cudnn_handle,
               cudnnTensorDescriptor_t &input_descriptor,
               cudnnTensorDescriptor_t &output_descriptor,
               const int filter_num,
               const int input_size,
               const int filter_size,
               const float filter_param[] )
  {
    number_of_filters = filter_num;
    number_of_input_planes = input_size;
    filter_height = filter_size;
    filter_width = filter_size;

    CheckCudnnErrors(cudnnCreateConvolutionDescriptor(&convolution_descriptor));
    CheckCudnnErrors(cudnnCreateFilterDescriptor(&filter_descriptor));
  
    const unsigned int data_size = number_of_filters * number_of_input_planes * filter_height * filter_width;
    const unsigned int required_memory_size = data_size * sizeof(float);

    std::cerr << "Filter Size : " << required_memory_size << " bytes" << std::endl;

    // Preparation for filters
    const int pad = (filter_size - 1) / 2;
    CheckCudaErrors(cudaMalloc((void**)&filter_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMemcpy(filter_data_gpu, filter_param, required_memory_size, cudaMemcpyHostToDevice));  
    CheckCudnnErrors(cudnnSetFilter4dDescriptor(filter_descriptor, CUDNN_DATA_FLOAT, CUDNN_TENSOR_NCHW, number_of_filters, number_of_input_planes, filter_height, filter_width));
    CheckCudnnErrors(cudnnSetConvolution2dDescriptor(convolution_descriptor, pad, pad, 1, 1, 1, 1, CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT));
    CheckCudnnErrors(cudnnGetConvolutionForwardAlgorithm(cudnn_handle,
                                                         input_descriptor,
                                                         filter_descriptor,
                                                         convolution_descriptor,
                                                         output_descriptor,
                                                         CUDNN_CONVOLUTION_FWD_PREFER_FASTEST,
                                                         0,
                                                         &forward_algorithm));
    std::cerr << "forward algorithm : " << forward_algorithm << std::endl;
    CheckCudnnErrors(cudnnGetConvolutionForwardWorkspaceSize(cudnn_handle,
                                                             input_descriptor,
                                                             filter_descriptor,
                                                             convolution_descriptor,
                                                             output_descriptor,
                                                             forward_algorithm,
                                                             &workspace_size));
    if (workspace_size != 0) {
      CheckCudaErrors(cudaMalloc(&workspace, workspace_size));
    }
  }

  ~Convolution()
  {

    if (filter_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(filter_data_gpu));
      filter_data_gpu = nullptr;
    }

    if (workspace != nullptr) {
      CheckCudaErrors(cudaFree(workspace));
      workspace = nullptr;
    }

    CheckCudnnErrors(cudnnDestroyConvolutionDescriptor(convolution_descriptor));
    CheckCudnnErrors(cudnnDestroyFilterDescriptor(filter_descriptor));
  }

  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &input_descriptor,
           cudnnTensorDescriptor_t &output_descriptor,
           float *input_ptr,
           float *output_ptr )
  {
    const float alpha = 1.0f;
    const float beta = 0.0f;

    CheckCudnnErrors(cudnnConvolutionForward(cudnn_handle,
                                             &alpha,
                                             input_descriptor,
                                             input_ptr,
                                             filter_descriptor,
                                             filter_data_gpu,
                                             convolution_descriptor,
                                             forward_algorithm,
                                              workspace,
                                             workspace_size,
                                             &beta,
                                             output_descriptor,
                                             output_ptr));
  }
};


class FullyConnected {
private:
  int in_size;
  int out_size;
  float *weight_data_gpu = nullptr;

public:
  
  FullyConnected( int input_size,
                  int output_size,
                  const float weight_param[] )
  {
    in_size = input_size;
    out_size = output_size;

    const unsigned int data_size = in_size * out_size;
    const unsigned int required_memory_size = data_size * sizeof(float);

    std::cerr << "Weight Size : " << required_memory_size << " bytes" << std::endl;

    CheckCudaErrors(cudaMalloc((void**)&weight_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMemcpy(weight_data_gpu, weight_param, required_memory_size, cudaMemcpyHostToDevice));
  }

  
  ~FullyConnected()
  {
    if (weight_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(weight_data_gpu));
      weight_data_gpu = nullptr;
    }
  }
  
  void Forward( cublasHandle_t &cublas_handle,
                float *input_ptr,
                float *output_ptr,
                const int batch_size )
  {
    const float alpha = 1.0f;
    const float beta = 0.0f;

    CheckCublasErrors(cublasGemmEx(cublas_handle,
                                   CUBLAS_OP_T,
                                   CUBLAS_OP_N,
                                   out_size,
                                   batch_size,
                                   in_size,
                                   &alpha,
                                   weight_data_gpu,
                                   CUDA_R_32F,
                                   in_size,
                                   input_ptr,
                                    CUDA_R_32F,
                                   in_size,
                                   &beta,
                                   output_ptr,
                                   CUDA_R_32F,
                                   out_size,
                                   CUDA_R_32F,
                                   CUBLAS_GEMM_DFALT));
  }

};


class BatchNormalization {
private:
  cudnnTensorDescriptor_t batchnorm_descriptor;
  float *scale_data_gpu = nullptr;
  float *bias_data_gpu = nullptr;
  float *mean_data_gpu = nullptr;
  float *variance_data_gpu = nullptr;

public:
  BatchNormalization( const int data_size,
                      const float scale_param[],
                      const float bias_param[],
                      const float mean_param[],
                      const float variance_param[] )
  {
    const unsigned int required_memory_size = sizeof(float) * data_size;

    CheckCudnnErrors(cudnnCreateTensorDescriptor(&batchnorm_descriptor));
  
    CheckCudaErrors(cudaMalloc((void**)&scale_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMalloc((void**)&bias_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMalloc((void**)&mean_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMalloc((void**)&variance_data_gpu, required_memory_size));

    CheckCudaErrors(cudaMemcpy(scale_data_gpu, scale_param, required_memory_size, cudaMemcpyHostToDevice));
    CheckCudaErrors(cudaMemcpy(bias_data_gpu, bias_param, required_memory_size, cudaMemcpyHostToDevice));
    CheckCudaErrors(cudaMemcpy(mean_data_gpu, mean_param, required_memory_size, cudaMemcpyHostToDevice));
    CheckCudaErrors(cudaMemcpy(variance_data_gpu, variance_param, required_memory_size, cudaMemcpyHostToDevice));
  }
  
  ~BatchNormalization()
  {
    if (scale_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(scale_data_gpu));
      scale_data_gpu = nullptr;
    }

    if (bias_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(bias_data_gpu));
      bias_data_gpu = nullptr;
    }

    if (mean_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(mean_data_gpu));
      mean_data_gpu = nullptr;
    }

    if (variance_data_gpu != nullptr) { 
      CheckCudaErrors(cudaFree(variance_data_gpu));
      variance_data_gpu = nullptr;
    }

    CheckCudnnErrors(cudnnDestroyTensorDescriptor(batchnorm_descriptor));
  }
    
  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *input_ptr,
           float *output_ptr )
  {
    const float alpha = 1.0f;
    const float beta = 0.0f;
    const double epsilon = 1e-5;

    CheckCudnnErrors(cudnnDeriveBNTensorDescriptor(batchnorm_descriptor,
                                                   tensor_descriptor,
                                                   CUDNN_BATCHNORM_SPATIAL));

    CheckCudnnErrors(cudnnBatchNormalizationForwardInference(cudnn_handle,
                                                             CUDNN_BATCHNORM_SPATIAL,
                                                             &alpha,
                                                             &beta,
                                                             tensor_descriptor,
                                                             input_ptr,
                                                             tensor_descriptor,
                                                             output_ptr,
                                                             batchnorm_descriptor,
                                                             scale_data_gpu,
                                                             bias_data_gpu,
                                                             mean_data_gpu,
                                                             variance_data_gpu,
                                                             epsilon));
  }
};


class Bias {
private:
  int number_of_biases;
  float *bias_data_gpu = nullptr;
  cudnnTensorDescriptor_t bias_descriptor;

public:
  Bias( cudnnHandle_t &cudnn_handle,
        const int num_of_biases,
        const float bias_param[] )
  {
    number_of_biases = num_of_biases;
    CheckCudnnErrors(cudnnCreateTensorDescriptor(&bias_descriptor));

    const unsigned int required_memory_size = number_of_biases * sizeof(float);

    std::cerr << "Bias Size : " << required_memory_size << " bytes" << std::endl;

    CheckCudaErrors(cudaMalloc((void**)&bias_data_gpu, required_memory_size));
    CheckCudaErrors(cudaMemcpy(bias_data_gpu, bias_param, required_memory_size, cudaMemcpyHostToDevice));
    CheckCudnnErrors(cudnnSetTensor4dDescriptor(bias_descriptor, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, 1, number_of_biases, 1, 1));
  }

  ~Bias()
  {
    if (bias_data_gpu != nullptr) {
      CheckCudaErrors(cudaFree(bias_data_gpu));
      bias_data_gpu = nullptr;
    }
    CheckCudnnErrors(cudnnDestroyTensorDescriptor(bias_descriptor));
  }

  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *tensor_ptr )
  {
    const float alpha = 1.0f;
    const float beta = 1.0f;

    CheckCudnnErrors(cudnnAddTensor(cudnn_handle,
                                    &alpha,
                                    bias_descriptor,
                                    bias_data_gpu,
                                    &beta,
                                    tensor_descriptor,
                                    tensor_ptr));
  }
};


class ReLU {
private:
  cudnnActivationDescriptor_t activation_descriptor;  

public:
  ReLU()
  {
    CheckCudnnErrors(cudnnCreateActivationDescriptor(&activation_descriptor));
    CheckCudnnErrors(cudnnSetActivationDescriptor(activation_descriptor, CUDNN_ACTIVATION_RELU, CUDNN_PROPAGATE_NAN, 0.0)); 
  }
  
  ~ReLU()
  {
    CheckCudnnErrors(cudnnDestroyActivationDescriptor(activation_descriptor));
  }

  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *tensor_ptr )
  {
    float alpha = 1.0f, beta = 0.0f;
  
    CheckCudnnErrors(cudnnActivationForward(cudnn_handle,
                                            activation_descriptor,
                                            &alpha,
                                            tensor_descriptor,
                                            tensor_ptr,
                                            &beta,
                                            tensor_descriptor,
                                            tensor_ptr));
  }
};


class Softmax {
public:  
  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *tensor_ptr )
  {
    const float alpha = 1.0f;
    const float beta = 0.0f;
    CheckCudnnErrors(cudnnSoftmaxForward(cudnn_handle,
                                   CUDNN_SOFTMAX_ACCURATE,
                                   CUDNN_SOFTMAX_MODE_CHANNEL,
                                   &alpha,
                                   tensor_descriptor,
                                   tensor_ptr,
                                   &beta,
                                   tensor_descriptor,
                                   tensor_ptr));
  }
};


class Tanh {
private:
  cudnnActivationDescriptor_t activation_descriptor;  

public:
  Tanh()
  {
    CheckCudnnErrors(cudnnCreateActivationDescriptor(&activation_descriptor));
    CheckCudnnErrors(cudnnSetActivationDescriptor(activation_descriptor, CUDNN_ACTIVATION_TANH, CUDNN_PROPAGATE_NAN, 0.0)); 
  }

  ~Tanh()
  {
    CheckCudnnErrors(cudnnDestroyActivationDescriptor(activation_descriptor));
  }
  
  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *tensor_ptr )
  {
    const float alpha = 1.0f;
    const float beta = 0.0f;
    
    CheckCudnnErrors(cudnnActivationForward(cudnn_handle,
                                            activation_descriptor,
                                            &alpha,
                                            tensor_descriptor,
                                            tensor_ptr,
                                            &beta,
                                            tensor_descriptor,
                                            tensor_ptr));
  }
};


class Add {
public:
  void
  Forward( cudnnHandle_t &cudnn_handle,
           cudnnTensorDescriptor_t &tensor_descriptor,
           float *x,
           float *y )
  {
    const float alpha = 1.0f;
    const float beta = 1.0f;

    CheckCudnnErrors(cudnnAddTensor(cudnn_handle, &alpha, tensor_descriptor, x, &beta, tensor_descriptor, y));
  }
};



struct ResParameter {
  float *conv1_weight;
  float *conv2_weight;
  float *bn1_scale;
  float *bn1_bias;
  float *bn1_mean;
  float *bn1_variance;
  float *bn2_scale;
  float *bn2_bias;
  float *bn2_mean;
  float *bn2_variance;
};


class ResBlock {
private:
  std::unique_ptr<Convolution> conv1 = nullptr;
  std::unique_ptr<Convolution> conv2 = nullptr;
  std::unique_ptr<BatchNormalization> bn1 = nullptr;
  std::unique_ptr<BatchNormalization> bn2 = nullptr;

public:
  ResBlock( CudaHandles &cuda_handles,
              cudnnTensorDescriptor_t &tensor_descriptor,
              ResParameter &res,
              const int filters )
  {
    conv1.reset(new Convolution(cuda_handles.cudnn, tensor_descriptor, tensor_descriptor, filters, filters, 3, res.conv1_weight));
    conv2.reset(new Convolution(cuda_handles.cudnn, tensor_descriptor, tensor_descriptor, filters, filters, 3, res.conv2_weight));
    bn1.reset(new BatchNormalization(filters, res.bn1_scale, res.bn1_bias, res.bn1_mean, res.bn1_variance));
    bn2.reset(new BatchNormalization(filters, res.bn2_scale, res.bn2_bias, res.bn2_mean, res.bn2_variance));
  }

  
  ~ResBlock()
  {
  }

  
  void Forward( CudaHandles &cuda_handles,
                cudnnTensorDescriptor_t &res_descriptor,
                float *input_ptr,
                float *hidden_ptr,
                float *res_ptr,
                float *output_ptr,
                std::unique_ptr<ReLU> &relu,
                std::unique_ptr<Add> &add,
                const int batch_size )
  {
    conv1->Forward(cuda_handles.cudnn, res_descriptor, res_descriptor, input_ptr, hidden_ptr);
    bn1->Forward(cuda_handles.cudnn, res_descriptor, hidden_ptr, res_ptr);
    relu->Forward(cuda_handles.cudnn, res_descriptor, res_ptr);
    conv2->Forward(cuda_handles.cudnn, res_descriptor, res_descriptor, res_ptr, hidden_ptr);
    bn2->Forward(cuda_handles.cudnn, res_descriptor, hidden_ptr, res_ptr);
    relu->Forward(cuda_handles.cudnn, res_descriptor, res_ptr);      
    add->Forward(cuda_handles.cudnn, res_descriptor, res_ptr, output_ptr);    
  }
};



#endif
