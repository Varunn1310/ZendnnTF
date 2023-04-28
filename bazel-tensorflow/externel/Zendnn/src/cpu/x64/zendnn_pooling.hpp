﻿/*******************************************************************************
* Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Modifications Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Copyright 2017-2019 Intel Corporation
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

#ifndef ZENDNN_POOLING_HPP
#define ZENDNN_POOLING_HPP

#include <assert.h>
#include <memory>

#include "common/c_types_map.hpp"
#include "common/zendnn_thread.hpp"
#include "common/primitive.hpp"
#include "common/type_helpers.hpp"
#include "common/utils.hpp"

#include "cpu/cpu_pooling_pd.hpp"
#include "cpu/x64/zendnn_pool_kernel.hpp"
#include "cpu/x64/jit_uni_reorder.hpp"

namespace zendnn {
namespace impl {
namespace cpu {
namespace x64 {

template <cpu_isa_t isa, impl::data_type_t d_type>
struct zendnn_pooling_fwd_t : public primitive_t {
    struct pd_t : public cpu_pooling_fwd_pd_t {
        using cpu_pooling_fwd_pd_t::cpu_pooling_fwd_pd_t;

        DECLARE_COMMON_PD_T("zendnn", zendnn_pooling_fwd_t);

        status_t init(engine_t *engine) {
            using namespace utils;

            const bool ok = true && set_default_params() == status::success
                      && is_fwd() && !has_zero_dim_memory()
                      && everyone_is(
                          d_type, src_md()->data_type, dst_md()->data_type)
                      && attr()->has_default_values()
                      && memory_desc_matches_tag(*src_md(), desired_fmt_tag())
                      && memory_desc_matches_tag(*dst_md(), desired_fmt_tag());
            if (!ok) return status::unimplemented;

            const bool is_training = desc_.prop_kind == prop_kind::forward_training;
            if (desc()->alg_kind == alg_kind::pooling_max && is_training)
                init_default_ws();

            auto scratchpad = scratchpad_registry().registrar();
            return zendnn_pool_kernel<isa>::init_conf(
                    jpp_, scratchpad, this, zendnn_get_max_threads());
        }

        format_tag_t desired_fmt_tag() {
            using namespace format_tag;
            return utils::one_of(isa, avx512_core)
                   ? (ndims() == 4 ? nChw16c : nCdhw16c)
                   : (ndims() == 4 ? nChw8c : nCdhw8c);
        }

        jit_pool_conf_t jpp_;
    };

    explicit zendnn_pooling_fwd_t(const pd_t *apd);
    zendnn_pooling_fwd_t(zendnn_pooling_fwd_t &&) =  default;
    zendnn_pooling_fwd_t &operator=(zendnn_pooling_fwd_t &&) = default; 
    ~zendnn_pooling_fwd_t();

    using data_t = typename prec_traits<d_type>::type;

    status_t init(engine_t *engine) override;

    status_t execute(const exec_ctx_t &ctx) const override {
        auto src = CTX_IN_MEM(const data_t *, ZENDNN_ARG_SRC);
        auto dst = CTX_OUT_MEM(data_t *, ZENDNN_ARG_DST);
        auto ws = CTX_OUT_MEM(char *, ZENDNN_ARG_WORKSPACE);

        execute_forward(src, dst, ws, ctx);

        return status::success;
    }

  private:
    void execute_forward(const data_t *src, data_t *dst, char *indices,
            const exec_ctx_t &ctx) const;
    const pd_t *pd() const { return (const pd_t *)primitive_t::pd().get(); }

    std::unique_ptr<zendnn_pool_kernel<isa>> kernel_;
    static constexpr data_type_t wsp_dt_ = data_type::f32;
};

} // namespace x64
} // namespace cpu
} // namespace impl
} // namespace zendnn

#endif

// vim: et ts=4 sw=4 cindent cino+=l0,\:4,N-s
