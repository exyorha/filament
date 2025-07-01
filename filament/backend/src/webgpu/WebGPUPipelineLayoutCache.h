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

#ifndef TNT_FILAMENT_BACKEND_WEBGPUPIPELINELAYOUTCACHE_H
#define TNT_FILAMENT_BACKEND_WEBGPUPIPELINELAYOUTCACHE_H

#include <backend/DriverEnums.h>

#include <utils/CString.h>
#include <utils/Hash.h>

#include <tsl/robin_map.h>
#include <webgpu/webgpu_cpp.h>

#include <array>
#include <cstdint>

namespace filament::backend {

class WebGPUPipelineLayoutCache final {
public:
    explicit WebGPUPipelineLayoutCache(wgpu::Device const&);
    WebGPUPipelineLayoutCache(WebGPUPipelineLayoutCache const&) = delete;
    WebGPUPipelineLayoutCache(WebGPUPipelineLayoutCache const&&) = delete;
    WebGPUPipelineLayoutCache& operator=(WebGPUPipelineLayoutCache const&) = delete;
    WebGPUPipelineLayoutCache& operator=(WebGPUPipelineLayoutCache const&&) = delete;

    [[nodiscard]] wgpu::PipelineLayout const& getOrCreatePipelineLayout(utils::CString const& label,
            std::array<wgpu::BindGroupLayout, MAX_DESCRIPTOR_SET_COUNT> const&,
            size_t bindGroupLayoutCount);

    void onFrameEnd();

private:
    struct PipelineLayoutKey final {                                            // size : offset (need multiples of 4 bytes for hashing)
        WGPUBindGroupLayout bindGroupLayoutHandles[MAX_DESCRIPTOR_SET_COUNT]{}; // 32   : 0
        uint8_t bindGroupLayoutCount{ 0 };                                      // 1    : 32
        uint8_t padding[7]{ 0 };                                                // 7    : 33
    };
    static_assert(sizeof(PipelineLayoutKey) == 40,
            "PipelineLayoutKey must not have implicit padding.");

    struct PipelineLayoutKeyEqual {
        bool operator()(PipelineLayoutKey const&, PipelineLayoutKey const&) const;
    };

    struct PipelineLayoutCacheEntry final {
        wgpu::PipelineLayout layout{ nullptr };
        uint64_t lastUsedFrameCount{ 0 };
    };

    static void populateKey(std::array<wgpu::BindGroupLayout, MAX_DESCRIPTOR_SET_COUNT> const&,
            size_t bindGroupLayoutCount, PipelineLayoutKey& outKey);

    [[nodiscard]] wgpu::PipelineLayout createPipelineLayout(utils::CString const& label,
            std::array<wgpu::BindGroupLayout, MAX_DESCRIPTOR_SET_COUNT> const&,
            size_t bindGroupLayoutCount);

    void collectGarbage();

    wgpu::Device const& mDevice;
    tsl::robin_map<PipelineLayoutKey, PipelineLayoutCacheEntry,
            utils::hash::MurmurHashFn<PipelineLayoutKey>, PipelineLayoutKeyEqual>
            mPipelineLayouts{};
    uint64_t mFrameCount{ 0 };
};

} // namespace filament::backend

#endif // TNT_FILAMENT_BACKEND_WEBGPUPIPELINELAYOUTCACHE_H
