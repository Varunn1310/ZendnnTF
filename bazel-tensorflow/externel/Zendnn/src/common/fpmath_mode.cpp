/*******************************************************************************
* Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Copyright 2021 Intel Corporation
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

#include "zendnn.h"

#include "c_types_map.hpp"
#include "utils.hpp"

namespace zendnn {
namespace impl {
static setting_t<fpmath_mode_t> default_fpmath {fpmath_mode::strict};

void init_fpmath_mode() {
    if (default_fpmath.initialized()) return;

    static std::string val = getenv_string_user("DEFAULT_FPMATH_MODE");
    if (!val.empty()) {
        if (val.compare("strict") == 0) default_fpmath.set(fpmath_mode::strict);
        if (val.compare("bf16") == 0) default_fpmath.set(fpmath_mode::bf16);
        if (val.compare("f16") == 0) default_fpmath.set(fpmath_mode::f16);
        if (val.compare("any") == 0) default_fpmath.set(fpmath_mode::any);
    }
    if (!default_fpmath.initialized()) default_fpmath.set(default_fpmath.get());
}

status_t check_fpmath_mode(fpmath_mode_t mode) {
    if (utils::one_of(mode, fpmath_mode::strict, fpmath_mode::bf16,
                fpmath_mode::f16, fpmath_mode::any))
        return status::success;
    return status::invalid_arguments;
}

bool is_fpsubtype(data_type_t sub_dt, data_type_t dt) {
    using namespace zendnn::impl::utils;
    using namespace zendnn::impl::data_type;
    switch (dt) {
        case f32: return one_of(sub_dt, f32, bf16, f16);
        case bf16: return one_of(sub_dt, bf16);
        case f16: return one_of(sub_dt, f16);
        default: return false;
    }
}

fpmath_mode_t get_fpmath_mode() {
    init_fpmath_mode();
    auto mode = default_fpmath.get();
    // Should always be proper, since no way to set invalid mode
    assert(check_fpmath_mode(mode) == status::success);
    return mode;
}

} // namespace impl
} // namespace zendnn

zendnn_status_t zendnn_set_default_fpmath_mode(zendnn_fpmath_mode_t mode) {
    using namespace zendnn::impl;
    auto st = check_fpmath_mode(mode);
    if (st == status::success) default_fpmath.set(mode);
    return st;
}

zendnn_status_t zendnn_get_default_fpmath_mode(zendnn_fpmath_mode_t *mode) {
    using namespace zendnn::impl;
    if (mode == nullptr) return status::invalid_arguments;

    auto m = get_fpmath_mode();
    // Should always be proper, since no way to set invalid mode
    auto st = check_fpmath_mode(m);
    if (st == status::success) *mode = m;
    return st;
}
