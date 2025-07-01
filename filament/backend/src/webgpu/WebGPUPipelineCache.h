/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_BACKEND_WEBGPUPIPELINECACHE_H
#define TNT_FILAMENT_BACKEND_WEBGPUPIPELINECACHE_H

#include "WebGPUVertexBufferInfo.h"

#include <backend/DriverEnums.h>

#include <utils/CString.h>
#include <utils/Hash.h>

#include <tsl/robin_map.h>
#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <vector>

namespace filament::backend {

class WebGPUPipelineCache final {
public:
    explicit WebGPUPipelineCache(wgpu::Device const&);
    WebGPUPipelineCache(WebGPUPipelineCache const&) = delete;
    WebGPUPipelineCache(WebGPUPipelineCache const&&) = delete;
    WebGPUPipelineCache& operator=(WebGPUPipelineCache const&) = delete;
    WebGPUPipelineCache& operator=(WebGPUPipelineCache const&&) = delete;

    [[nodiscard]] wgpu::RenderPipeline const& getOrCreateRenderPipeline(utils::CString const& label,
            wgpu::ShaderModule const& vertexShaderModule,
            wgpu::ShaderModule const& fragmentShaderModule,
            std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const&,
            wgpu::VertexBufferLayout const*, wgpu::PipelineLayout const&);

    void onFrameEnd();

private:
    struct VertexAttribute final {       // size : offset (need multiples of 4 bytes for hashing)
        uint8_t bufferIndex{ 0 };        // 1    : 0
        // this is the webgpu offset,
        // bytes from the start of
        // the vertex data
        uint8_t offset{ 0 };             // 1    : 1
        uint8_t shaderLocation{ 0 };     // 1    : 2
        uint8_t padding { 0 };           // 1    : 3
        wgpu::VertexFormat format{ 0 };  // 4    : 4
    };
    static_assert(sizeof(VertexAttribute) == 8, "VertexAttribute must not have implicit padding.");

    struct VertexBuffer final {  // size : offset (need multiples of 4 bytes for hashing)
        uint8_t stride{ 0 };     // 1    : 0
        uint8_t padding[3]{ 0 }; // 3    : 1
        // offset in bytes from
        // the start of the
        // GPU buffer
        uint32_t offset{ 0 };    // 4    : 4
    };
    static_assert(sizeof(VertexBuffer) == 8, "VertexBuffer must not have implicit padding.");

    struct RenderPipelineKey final {                                    // size : offset (need multiples of 4 bytes for hashing)
        WGPUShaderModule vertexShaderModuleHandle{ nullptr };           // 8    : 0
        WGPUShaderModule fragmentShaderModuleHandle{ nullptr };         // 8    : 8
        VertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTE_COUNT]{}; // 128  : 16
        VertexBuffer vertexBuffers[MAX_VERTEX_BUFFER_COUNT]{};          // 128  : 144
        WGPUPipelineLayout pipelineLayout{ nullptr };                   // 8    : 272
    };
    static_assert(sizeof(RenderPipelineKey) == 280,
            "RenderPipelineKey must not have implicit padding.");

    struct RenderPipelineKeyEqual {
        bool operator()(RenderPipelineKey const&, RenderPipelineKey const&) const;
    };

    struct RenderPipelineCacheEntry final {
        wgpu::RenderPipeline pipeline{ nullptr };
        uint64_t lastUsedFrameCount{ 0 };
    };

    static void populateKey(wgpu::ShaderModule const& vertexShaderModule,
            wgpu::ShaderModule const& fragmentShaderModule,
            std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const&,
            wgpu::VertexBufferLayout const*, wgpu::PipelineLayout const&,
            RenderPipelineKey& outKey);

    [[nodiscard]] wgpu::RenderPipeline createRenderPipeline(utils::CString const& label,
            wgpu::ShaderModule const& vertexShaderModule,
            wgpu::ShaderModule const& fragmentShaderModule,
            std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const&,
            wgpu::VertexBufferLayout const*, wgpu::PipelineLayout const&);

    void collectGarbage();

    wgpu::Device const& mDevice;
    tsl::robin_map<RenderPipelineKey, RenderPipelineCacheEntry,
            utils::hash::MurmurHashFn<RenderPipelineKey>, RenderPipelineKeyEqual>
            mRenderPipelines{};
    uint64_t mFrameCount{ 0 };
};

} // namespace filament::backend

#endif // TNT_FILAMENT_BACKEND_WEBGPUPIPELINECACHE_H
