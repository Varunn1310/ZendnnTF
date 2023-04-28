/*******************************************************************************
* Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Copyright 2020-2022 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cpu/cpu_engine.hpp"
#include "cpu/ref_prelu.hpp"

#if ZENDNN_X64
#include "cpu/x64/prelu/jit_prelu_backward.hpp"
#include "cpu/x64/prelu/jit_prelu_forward.hpp"

using namespace zendnn::impl::cpu::x64;
#endif

namespace zendnn {
namespace impl {
namespace cpu {

namespace {
using namespace zendnn::impl::data_type;
using namespace zendnn::impl::prop_kind;

// clang-format off
const std::map<pk_impl_key_t, std::vector<impl_list_item_t>> &impl_list_map() {
    static const std::map<pk_impl_key_t, std::vector<impl_list_item_t>> the_map = REG_PRELU_P({
        {{forward}, {
            CPU_INSTANCE_X64(jit_prelu_fwd_t)
            CPU_INSTANCE(ref_prelu_fwd_t)
            nullptr,
        }},
        {{backward}, REG_BWD_PK({
            CPU_INSTANCE_X64(jit_prelu_bwd_t)
            CPU_INSTANCE(ref_prelu_bwd_t)
            nullptr,
        })},
    });
    return the_map;
}
// clang-format on
} // namespace

const impl_list_item_t *get_prelu_impl_list(const prelu_desc_t *desc) {
    static const impl_list_item_t empty_list[] = {nullptr};

    const bool is_fwd = utils::one_of(
            desc->prop_kind, forward_training, forward_inference);
    prop_kind_t prop_kind = is_fwd ? forward : backward;

    pk_impl_key_t key {prop_kind};

    const auto impl_list_it = impl_list_map().find(key);
    return impl_list_it != impl_list_map().cend() ? impl_list_it->second.data()
                                                  : empty_list;
}

} // namespace cpu
} // namespace impl
} // namespace zendnn