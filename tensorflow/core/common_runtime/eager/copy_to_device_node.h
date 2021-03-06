/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_EAGER_COPY_TO_DEVICE_NODE_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_EAGER_COPY_TO_DEVICE_NODE_H_

#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/eager/eager_executor.h"
#include "tensorflow/core/common_runtime/eager/tensor_handle.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {

class CopyToDeviceNode : public EagerNode {
 public:
  CopyToDeviceNode(TensorHandle* src, Device* dstd, EagerContext* ctx)
      : EagerNode(ctx->NextId()), src_(src), dstd_(dstd), ctx_(ctx) {
    src_->Ref();
    status_ = TensorHandle::CreateAsyncLocalHandle(dstd_, dstd_, nullptr,
                                                   src_->dtype, ctx, &dst_);
    if (status_.ok()) {
      dst_->Ref();
    }
  }

  ~CopyToDeviceNode() override {
    src_->Unref();
    if (dst_) {
      dst_->Unref();
    }
  }

  Status Run() override {
    if (!status_.ok()) {
      return status_;
    }
    TensorHandle* temp = nullptr;
    Status status = src_->CopyToDevice(ctx_, dstd_, &temp);
    if (!status.ok()) {
      dst_->Poison(status);
      return status;
    }
    const Tensor* tensor = nullptr;
    status = temp->Tensor(&tensor);
    // `temp` is a ready handle. So the following call should return OK.
    TF_DCHECK_OK(status) << status.error_message();
    DCHECK(tensor);
    dst_->SetTensor(*tensor);
    temp->Unref();
    return Status::OK();
  }

  void Abort(Status status) override { dst_->Poison(status); }

  TensorHandle* dst() { return dst_; }

 private:
  TensorHandle* src_;
  Device* dstd_;
  EagerContext* ctx_;
  TensorHandle* dst_;
  Status status_;
};

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_EAGER_COPY_TO_DEVICE_NODE_H_
