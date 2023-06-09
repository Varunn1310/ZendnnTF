/*******************************************************************************
* Modifications Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
* Notified per clause 4(b) of the license.
*******************************************************************************/

/*******************************************************************************
* Copyright 2020 Intel Corporation
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

#ifndef ZENDNN_SYCL_H
#define ZENDNN_SYCL_H

#include "zendnn.h"

#include "zendnn_sycl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup zendnn_api
/// @{

/// @addtogroup zendnn_api_interop
/// @{

/// @addtogroup zendnn_api_sycl_interop
/// @{

/// Creates an engine associated with a SYCL device and a SYCL context.
///
/// @param engine Output engine.
/// @param device Pointer to the SYCL device to use for the engine.
/// @param context Pointer to the SYCL context to use for the engine.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_engine_create(
        zendnn_engine_t *engine, const void *device, const void *context);

/// Returns the SYCL context associated with an engine.
///
/// @param engine Engine to query.
/// @param context Pointer to the underlying SYCL context of the engine.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_engine_get_context(
        zendnn_engine_t engine, void **context);

/// Returns the SYCL device associated with an engine.
///
/// @param engine Engine to query.
/// @param device Pointer to the underlying SYCL device of the engine.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_engine_get_device(
        zendnn_engine_t engine, void **device);

/// Creates a memory object.
///
/// Unless @p handle is equal to ZENDNN_MEMORY_NONE or ZENDNN_MEMORY_ALLOCATE, the
/// constructed memory object will have the underlying buffer set. In this
/// case, the buffer will be initialized as if:
/// - zendnn_memory_set_data_handle() had been called, if @p memory_kind is equal
///   to zendnn_sycl_interop_usm, or
/// - zendnn_sycl_interop_memory_set_buffer() has been called, if @p memory_kind
///   is equal to zendnn_sycl_interop_buffer.
///
/// @param memory Output memory object.
/// @param memory_desc Memory descriptor.
/// @param engine Engine to use.
/// @param memory_kind Memory allocation kind to specify the type of handle.
/// @param handle Handle of the memory buffer to use as an underlying storage.
///     - A USM pointer to the user-allocated buffer. In this case the library
///       doesn't own the buffer. Requires @p memory_kind to be equal to
///       zendnn_sycl_interop_usm.
///     - A pointer to SYCL buffer. In this case the library doesn't own the
///       buffer. Requires @p memory_kind be equal to be equal to
///       zendnn_sycl_interop_buffer.
///     - The ZENDNN_MEMORY_ALLOCATE special value. Instructs the library to
///       allocate the buffer that corresponds to the memory allocation kind
///       @p memory_kind for the memory object. In this case the library
///       owns the buffer.
///     - The ZENDNN_MEMORY_NONE specific value. Instructs the library to
///       create memory object without an underlying buffer.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_memory_create(zendnn_memory_t *memory,
        const zendnn_memory_desc_t *memory_desc, zendnn_engine_t engine,
        zendnn_sycl_interop_memory_kind_t memory_kind, void *handle);

/// Returns the memory allocation kind associated with a memory object.
///
/// @param memory Memory to query.
/// @param memory_kind Output underlying memory allocation kind of the memory
///     object.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_memory_get_memory_kind(
        const_zendnn_memory_t memory,
        zendnn_sycl_interop_memory_kind_t *memory_kind);

/// Sets a SYCL buffer for a memory object.
///
/// @param memory Memory object.
/// @param buffer SYCL buffer to be set in the memory object.
/// @param stream Stream to use to execute padding in.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_memory_set_buffer(
        zendnn_memory_t memory, void *buffer, zendnn_stream_t stream);

/// Creates an execution stream for a given engine associated with a SYCL
/// queue.
///
/// @param stream Output execution stream.
/// @param engine Engine to create the execution stream on.
/// @param queue SYCL queue to use.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_stream_create(
        zendnn_stream_t *stream, zendnn_engine_t engine, void *queue);

/// Returns the SYCL queue associated with an execution stream.
///
/// @param stream Execution stream to query.
/// @param queue Output SYCL command queue.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_stream_get_queue(
        zendnn_stream_t stream, void **queue);

/// Executes computations specified by the primitive in a specified stream and
/// returns a SYCL event.
///
/// @param primitive Primitive to execute.
/// @param stream Stream to use.
/// @param nargs Number of arguments.
/// @param args Array of arguments. Each argument is an
///     <index, #zendnn_memory_t> pair. The index is one of the `ZENDNN_ARG_*`
///     values such as `ZENDNN_ARG_SRC`. Unless runtime shapes are used (see
///     #ZENDNN_RUNTIME_DIM_VAL), the memory object must have the same memory
///     descriptor as that returned by
///     #zendnn_primitive_desc_query_md(#zendnn_query_exec_arg_md, index).
/// @param deps A pointer to std::vector<sycl::event> that contains
///     dependencies.
/// @param return_event Output event.
/// @returns #zendnn_success on success and a status describing the error
///     otherwise.
zendnn_status_t ZENDNN_API zendnn_sycl_interop_primitive_execute(
        const_zendnn_primitive_t primitive, zendnn_stream_t stream, int nargs,
        const zendnn_exec_arg_t *args, const void *deps, void *return_event);

/// @} zendnn_api_sycl_interop

/// @} zendnn_api_interop

/// @} zendnn_api

#ifdef __cplusplus
}
#endif

#endif
