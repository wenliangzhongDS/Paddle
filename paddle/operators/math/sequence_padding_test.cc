/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/operators/math/sequence_padding.h"
#include <gtest/gtest.h>

template <typename DeviceContext, typename Place, typename T>
void TestSequencePadding(const paddle::framework::LoD& lod,
                         const size_t sequence_width) {
  paddle::framework::LoDTensor cpu_seq;
  paddle::framework::LoDTensor cpu_seq_back;
  paddle::framework::LoDTensor seq;
  paddle::framework::LoDTensor seq_back;
  paddle::framework::Tensor padding;

  const size_t level = lod.size() - 1;
  auto seq_dims =
      paddle::framework::make_ddim({static_cast<int64_t>(lod[level].back()),
                                    static_cast<int64_t>(sequence_width)});

  cpu_seq.set_lod(lod);
  cpu_seq.mutable_data<T>(seq_dims, paddle::platform::CPUPlace());
  for (int64_t i = 0; i < cpu_seq.numel(); ++i) {
    cpu_seq.data<T>()[i] = static_cast<T>(i);
  }

  auto* place = new Place();
  DeviceContext* context = new DeviceContext(*place);
  if (paddle::platform::is_cpu_place(*place)) {
    seq = cpu_seq;
  } else {
    Copy(cpu_seq, *place, *context, &seq);
    seq.set_lod(lod);
  }

  const size_t max_sequence_length =
      paddle::operators::math::MaximumSequenceLength(lod, level);
  const size_t num_sequences = lod[level].size() - 1;
  auto padding_dims =
      paddle::framework::make_ddim({static_cast<int64_t>(max_sequence_length),
                                    static_cast<int64_t>(num_sequences),
                                    static_cast<int64_t>(sequence_width)});
  padding.mutable_data<T>(padding_dims, *place);
  paddle::operators::math::PaddingLoDTensorFunctor<DeviceContext, T>()(
      *context, seq, padding, false);

  seq_back.set_lod(lod);
  seq_back.mutable_data<T>(seq_dims, *place);
  paddle::operators::math::UnpaddingLoDTensorFunctor<DeviceContext, T>()(
      *context, seq_back, padding, false);

  if (paddle::platform::is_cpu_place(*place)) {
    cpu_seq_back = seq_back;
  } else {
    Copy(seq_back, paddle::platform::CPUPlace(), *context, &cpu_seq_back);
    cpu_seq_back.set_lod(lod);
  }

  EXPECT_EQ(cpu_seq.numel(), cpu_seq_back.numel());
  EXPECT_EQ(cpu_seq.dims(), cpu_seq_back.dims());
  for (int64_t i = 0; i < cpu_seq.numel(); ++i) {
    EXPECT_EQ(cpu_seq.data<T>()[i], cpu_seq_back.data<T>()[i]);
  }

  delete place;
  delete context;
};

TEST(Seq2BatchPadding, CPU) {
  paddle::framework::LoD lod1;
  lod1.push_back(std::vector<size_t>{0, 10});
  TestSequencePadding<paddle::platform::CPUDeviceContext,
                      paddle::platform::CPUPlace, float>(lod1, 16);

  paddle::framework::LoD lod2;
  lod2.push_back(std::vector<size_t>{0, 2, 7, 10});
  TestSequencePadding<paddle::platform::CPUDeviceContext,
                      paddle::platform::CPUPlace, float>(lod2, 128);
}

#ifdef PADDLE_WITH_CUDA
TEST(SequencePadding, CUDA) {
  paddle::framework::LoD lod1;
  lod1.push_back(std::vector<size_t>{0, 10});
  TestSequencePadding<paddle::platform::CUDADeviceContext,
                      paddle::platform::CUDAPlace, float>(lod1, 16);

  paddle::framework::LoD lod2;
  lod2.push_back(std::vector<size_t>{0, 2, 7, 10});
  TestSequencePadding<paddle::platform::CUDADeviceContext,
                      paddle::platform::CUDAPlace, float>(lod2, 128);
}
#endif
