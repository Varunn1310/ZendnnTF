/*******************************************************************************
* Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Copyright 2018-2022 Intel Corporation
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

#include "common/bfloat16.hpp"
#include "common/c_types_map.hpp"
#include "common/zendnn_thread.hpp"
#include "common/zendnn_traits.hpp"
#include "common/nstl.hpp"
#include "common/utils.hpp"

#include "cpu/gemm/gemm.hpp"
#include "cpu/gemm/gemm_msan_unpoison.hpp"
#include "cpu/gemm/os_blas.hpp"

#include "cpu/gemm/f32/ref_gemm_f32.hpp"
#include "cpu/gemm/s8x8s32/ref_gemm_s8x8s32.hpp"
#include "cpu/gemm/s8x8s32/simple_gemm_s8s8s32.hpp"

#if ZENDNN_X64
#include "cpu/x64/cpu_isa_traits.hpp"

#include "cpu/x64/gemm/f32/jit_avx512_common_gemm_f32.hpp"
#include "cpu/x64/gemm/f32/jit_avx_gemm_f32.hpp"

#include "cpu/x64/gemm/gemm_driver.hpp"

using namespace zendnn::impl::cpu::x64;
#endif

namespace zendnn {
namespace impl {
namespace cpu {

zendnn_status_t check_gemm_input(const char *transa, const char *transb,
        const dim_t *M, const dim_t *N, const dim_t *K, const void *A,
        const dim_t *lda, const void *B, const dim_t *ldb, const void *C,
        const dim_t *ldc, const float *alpha, const float *beta,
        const bool with_bias) {
    if (utils::any_null(
                transa, transb, M, N, K, A, lda, B, ldb, C, ldc, alpha, beta))
        return zendnn_invalid_arguments;
    if (with_bias && *beta != 0) return zendnn_unimplemented;
    bool consistency = true
            && utils::one_of(*transa, 'T', 't', 'N', 'n', 'P', 'p')
            && utils::one_of(*transb, 'T', 't', 'N', 'n', 'P', 'p') && *M >= 0
            && *N >= 0 && *K >= 0;

    if (!consistency) return zendnn_invalid_arguments;

    bool is_packed_a = utils::one_of(*transa, 'P', 'p');
    bool is_packed_b = utils::one_of(*transb, 'P', 'p');
    bool is_trans_a = utils::one_of(*transa, 'T', 't');
    bool is_trans_b = utils::one_of(*transb, 'T', 't');
    dim_t nrow_a = is_trans_a ? *K : *M;
    dim_t nrow_b = is_trans_b ? *N : *K;
    consistency = true && (is_packed_a || *lda >= nstl::max(dim_t(1), nrow_a))
            && (is_packed_b || *ldb >= nstl::max(dim_t(1), nrow_b))
            && *ldc >= nstl::max(dim_t(1), *M);
    if (!consistency) return zendnn_invalid_arguments;

    return zendnn_success;
}

zendnn_status_t check_gemm_x8x8x32_input(const char *offsetc, const char *transa,
        const char *transb, const dim_t *M, const dim_t *N, const dim_t *K,
        const void *A, const dim_t *lda, const void *B, const dim_t *ldb,
        const void *C, const dim_t *ldc, const float *alpha, const float *beta,
        const bool with_bias) {
    if (offsetc == nullptr) return zendnn_invalid_arguments;
    if (!utils::one_of(*offsetc, 'F', 'f', 'C', 'c', 'R', 'r'))
        return zendnn_invalid_arguments;

    return check_gemm_input(transa, transb, M, N, K, A, lda, B, ldb, C, ldc,
            alpha, beta, with_bias);
}

zendnn_status_t extended_sgemm(const char *transa, const char *transb,
        const dim_t *M, const dim_t *N, const dim_t *K, const float *alpha,
        const float *A, const dim_t *lda, const float *B, const dim_t *ldb,
        const float *beta, float *C, const dim_t *ldc, const float *bias,
        const bool force_jit_nocopy_gemm) {
    zendnn_status_t status = check_gemm_input(transa, transb, M, N, K, A, lda, B,
            ldb, C, ldc, alpha, beta, bias != nullptr);
    if (status != zendnn_success) return status;

#ifdef USE_CBLAS
    if (!force_jit_nocopy_gemm && utils::one_of(*transa, 'n', 'N', 't', 'T')
            && utils::one_of(*transb, 'n', 'N', 't', 'T')) {
        bool trA = *transa == 't' || *transa == 'T';
        bool trB = *transb == 't' || *transb == 'T';
        CBLAS_TRANSPOSE Cblas_trA = trA ? CblasTrans : CblasNoTrans;
        CBLAS_TRANSPOSE Cblas_trB = trB ? CblasTrans : CblasNoTrans;
        cblas_sgemm(CblasColMajor, Cblas_trA, Cblas_trB, *M, *N, *K, *alpha, A,
                *lda, B, *ldb, *beta, C, *ldc);
        if (bias) {
            // Add bias if necessary (bias is applied to columns of C)
            dim_t incx = 1, incy = 1;
            parallel_nd(*N, [&](dim_t n) {
                dim_t offset = n * (*ldc);
                cblas_saxpy(*M, 1.0, bias, incx, C + offset, incy);
            });
        }
        msan_unpoison_matrix(C, *M, *N, *ldc, sizeof(*C));
        return zendnn_success;
    }
#endif

#if ZENDNN_X64
    if (mayiuse(sse41)) {
        float *dummy_ao = nullptr;
        float *dummy_bo = nullptr;
        return gemm_driver(transa, transb, bias ? "C" : nullptr, M, N, K, alpha,
                A, lda, dummy_ao, B, ldb, dummy_bo, beta, C, ldc, bias,
                force_jit_nocopy_gemm);
    }
#endif

    return ref_gemm<float>(
            transa, transb, M, N, K, alpha, A, lda, B, ldb, beta, C, ldc, bias);
}

// Tries calling Intel MKL cblas_gemm_s8u8s32 if applicable and available
zendnn_status_t try_cblas_gemm_s8u8s32(const char *transa, const char *transb,
        const char *offsetc, const dim_t *M, const dim_t *N, const dim_t *K,
        const float *alpha, const int8_t *A, const dim_t *LDA, const int8_t *ao,
        const uint8_t *B, const dim_t *LDB, const uint8_t *bo,
        const float *beta, int32_t *C, const dim_t *LDC, const int32_t *co) {
#if USE_MKL_IGEMM
    // cblas_gemm_s8u8s32 uses `+` to apply offsets,
    // hence we need to inverse ao and b0.
    if (*ao == -128 || *bo > 128) return zendnn_unimplemented;

    assert(-127 <= *ao && *ao <= 127);
    assert(*bo <= 128);

    int8_t ao_s8 = -(*ao);
    int8_t bo_s8 = (int8_t)(-(int32_t)*bo);

    bool OCisR = (*offsetc == 'R' || *offsetc == 'r');
    bool OCisC = (*offsetc == 'C' || *offsetc == 'c');
    bool AisN = (*transa == 'N' || *transa == 'n');
    bool BisN = (*transb == 'N' || *transb == 'n');

    CBLAS_TRANSPOSE Cblas_trA = AisN ? CblasNoTrans : CblasTrans;
    CBLAS_TRANSPOSE Cblas_trB = BisN ? CblasNoTrans : CblasTrans;
    CBLAS_OFFSET Cblas_offsetc = OCisR
            ? CblasRowOffset
            : (OCisC ? CblasColOffset : CblasFixOffset);
    cblas_gemm_s8u8s32(CblasColMajor, Cblas_trA, Cblas_trB, Cblas_offsetc, *M,
            *N, *K, *alpha, A, *LDA, ao_s8, B, *LDB, bo_s8, *beta, C, *LDC, co);
    msan_unpoison_matrix(C, *M, *N, *LDC, sizeof(*C));
    return zendnn_success;
#else
    return zendnn_unimplemented;
#endif
}

template <>
zendnn_status_t gemm_s8x8s32(const char *transa, const char *transb,
        const char *offsetc, const dim_t *M, const dim_t *N, const dim_t *K,
        const float *alpha, const int8_t *A, const dim_t *LDA, const int8_t *ao,
        const uint8_t *B, const dim_t *LDB, const uint8_t *bo,
        const float *beta, int32_t *C, const dim_t *LDC, const int32_t *co) {
    zendnn_status_t status = check_gemm_x8x8x32_input(offsetc, transa, transb, M,
            N, K, A, LDA, B, LDB, C, LDC, alpha, beta, false);
    if (status != zendnn_success) return status;

    if (*M == 0 || *N == 0 || *K == 0) return zendnn_success;

    status = try_cblas_gemm_s8u8s32(transa, transb, offsetc, M, N, K, alpha, A,
            LDA, ao, B, LDB, bo, beta, C, LDC, co);
    if (status == zendnn_success) return status;

#if ZENDNN_X64
    if (mayiuse(sse41))
        return gemm_driver(transa, transb, offsetc, M, N, K, alpha, A, LDA, ao,
                B, LDB, bo, beta, C, LDC, co, false);
#endif

    return ref_gemm_s8x8s32(transa, transb, offsetc, M, N, K, alpha, A, LDA, ao,
            B, LDB, bo, beta, C, LDC, co);
}

template <>
zendnn_status_t gemm_s8x8s32(const char *transa, const char *transb,
        const char *offsetc, const dim_t *M, const dim_t *N, const dim_t *K,
        const float *alpha, const int8_t *A, const dim_t *LDA, const int8_t *ao,
        const int8_t *B, const dim_t *LDB, const int8_t *bo, const float *beta,
        int32_t *C, const dim_t *LDC, const int32_t *co) {
    zendnn_status_t status = check_gemm_x8x8x32_input(offsetc, transa, transb, M,
            N, K, A, LDA, B, LDB, C, LDC, alpha, beta, false);
    if (status != zendnn_success) return status;

    if (*M == 0 || *N == 0 || *K == 0) return zendnn_success;

#if ZENDNN_X64
    bool use_jit = mayiuse(avx512_core);
    bool use_s8u8 = true
            && utils::everyone_is(0, *ao, *bo) // so far a requirement
            && IMPLICATION(USE_MKL_IGEMM == 0, mayiuse(sse41));

    if (use_jit)
        return gemm_driver(transa, transb, offsetc, M, N, K, alpha, A, LDA, ao,
                B, LDB, bo, beta, C, LDC, co, false);
    else if (use_s8u8)
        return simple_gemm_s8s8s32(transa, transb, offsetc, M, N, K, alpha, A,
                LDA, ao, B, LDB, bo, beta, C, LDC, co);
#endif

    return ref_gemm_s8x8s32(transa, transb, offsetc, M, N, K, alpha, A, LDA, ao,
            B, LDB, bo, beta, C, LDC, co);
}

zendnn_status_t gemm_bf16bf16f32(const char *transa, const char *transb,
        const dim_t *M, const dim_t *N, const dim_t *K, const float *alpha,
        const bfloat16_t *A, const dim_t *lda, const bfloat16_t *B,
        const dim_t *ldb, const float *beta, float *C, const dim_t *ldc) {
    zendnn_status_t status = check_gemm_input(transa, transb, M, N, K, A, lda, B,
            ldb, C, ldc, alpha, beta, false);
    if (status != zendnn_success) return status;

#if ZENDNN_X64
    char *dummyOffsetC = nullptr;
    bfloat16_t *dummy_ao = nullptr;
    bfloat16_t *dummy_bo = nullptr;
    float *dummy_co = nullptr;

    if (mayiuse(avx512_core))
        return gemm_driver(transa, transb, dummyOffsetC, M, N, K, alpha,
                (const bfloat16_t *)A, lda, dummy_ao, (const bfloat16_t *)B,
                ldb, dummy_bo, beta, (float *)C, ldc, dummy_co, false);
#endif

    return zendnn_unimplemented;
}

} // namespace cpu
} // namespace impl
} // namespace zendnn
