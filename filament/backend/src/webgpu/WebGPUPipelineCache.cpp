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

#include "WebGPUPipelineCache.h"

#include "WebGPUConstants.h"
#include "WebGPUVertexBufferInfo.h"

#include <utils/CString.h>
#include <utils/Panic.h>

#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace filament::backend {

WebGPUPipelineCache::WebGPUPipelineCache(wgpu::Device const& device)
    : mDevice{ device } {}

wgpu::RenderPipeline const& WebGPUPipelineCache::getOrCreateRenderPipeline(
        utils::CString const& label, wgpu::ShaderModule const& vertexShaderModule,
        wgpu::ShaderModule const& fragmentShaderModule,
        std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const& vertexBufferSlots,
        wgpu::VertexBufferLayout const* vertexBufferLayouts,
        wgpu::PipelineLayout const& pipelineLayout) {
    RenderPipelineKey key{};
    populateKey(vertexShaderModule, fragmentShaderModule, vertexBufferSlots, vertexBufferLayouts,
            pipelineLayout, key);
    if (auto iterator{ mRenderPipelines.find(key) }; iterator != mRenderPipelines.end()) {
        RenderPipelineCacheEntry& entry{ iterator.value() };
        entry.lastUsedFrameCount = mFrameCount;
        return entry.pipeline;
    }
    const wgpu::RenderPipeline pipeline{ createRenderPipeline(label, vertexShaderModule,
            fragmentShaderModule, vertexBufferSlots, vertexBufferLayouts, pipelineLayout) };
    mRenderPipelines.emplace(key, RenderPipelineCacheEntry{
                                      .pipeline = pipeline,
                                      .lastUsedFrameCount = mFrameCount,
                                  });
    return mRenderPipelines[key].pipeline;
}

void WebGPUPipelineCache::onFrameEnd() {
    ++mFrameCount;
    collectGarbage();
}

void WebGPUPipelineCache::populateKey(const wgpu::ShaderModule& vertexShaderModule,
        const wgpu::ShaderModule& fragmentShaderModule,
        std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const& vertexBufferSlots,
        wgpu::VertexBufferLayout const* vertexBufferLayouts,
        wgpu::PipelineLayout const& pipelineLayout, RenderPipelineKey& outKey) {
    outKey.vertexShaderModuleHandle = vertexShaderModule.Get();
    outKey.fragmentShaderModuleHandle = fragmentShaderModule.Get();
    outKey.pipelineLayout = pipelineLayout.Get();
    uint8_t previousBufferIndex{ 0 };
    uint8_t currentAttributeIndex{ 0 };
    for (WebGPUVertexBufferInfo::WebGPUSlotBindingInfo const& vertexBufferSlot: vertexBufferSlots) {
        assert_invariant(previousBufferIndex <= vertexBufferSlot.sourceBufferIndex &&
                         "expecting monotonically increasing vertex buffer indices for consistent "
                         "key storage");
        outKey.vertexBuffers[vertexBufferSlot.sourceBufferIndex].stride = vertexBufferSlot.stride;
        outKey.vertexBuffers[vertexBufferSlot.sourceBufferIndex].offset =
                vertexBufferSlot.bufferOffset;
        wgpu::VertexBufferLayout const& vertexBufferLayout{
            vertexBufferLayouts[vertexBufferSlot.sourceBufferIndex]
        };
        for (size_t attributeIndex{ 0 }; attributeIndex < vertexBufferLayout.attributeCount;
                attributeIndex++) {
            wgpu::VertexAttribute const& vertexAttribute{
                vertexBufferLayout.attributes[attributeIndex]
            };
            outKey.vertexAttributes[currentAttributeIndex].bufferIndex =
                    vertexBufferSlot.sourceBufferIndex;
            outKey.vertexAttributes[currentAttributeIndex].offset =
                    static_cast<uint8_t>(vertexAttribute.offset);
            outKey.vertexAttributes[currentAttributeIndex].shaderLocation =
                    static_cast<uint8_t>(vertexAttribute.shaderLocation);
            outKey.vertexAttributes[currentAttributeIndex].format = vertexAttribute.format;
            currentAttributeIndex++;
        }
        previousBufferIndex = vertexBufferSlot.sourceBufferIndex;
    }
}

wgpu::RenderPipeline WebGPUPipelineCache::createRenderPipeline(utils::CString const& label,
        wgpu::ShaderModule const& vertexShaderModule,
        wgpu::ShaderModule const& fragmentShaderModule,
        std::vector<WebGPUVertexBufferInfo::WebGPUSlotBindingInfo> const& vertexBufferSlots,
        wgpu::VertexBufferLayout const* vertexBufferLayouts,
        wgpu::PipelineLayout const& pipelineLayout) {
    wgpu::RenderPipelineDescriptor pipelineDescriptor{
        .label = wgpu::StringView(label.c_str_safe()),
    };
    const wgpu::RenderPipeline pipeline{ mDevice.CreateRenderPipeline(&pipelineDescriptor) };
    FILAMENT_CHECK_POSTCONDITION(pipeline)
            << "Failed to create render pipeline for " << pipelineDescriptor.label;
    return pipeline;
}

bool WebGPUPipelineCache::RenderPipelineKeyEqual::operator()(RenderPipelineKey const& key1,
        RenderPipelineKey const& key2) const {
    return 0 == memcmp(reinterpret_cast<void const*>(&key1), reinterpret_cast<void const*>(&key2),
                        sizeof(key1));
}

void WebGPUPipelineCache::collectGarbage() {
    // remove any expired pipelines...
    using Iterator = decltype(mRenderPipelines)::const_iterator;
    for (Iterator iterator{ mRenderPipelines.begin() }; iterator != mRenderPipelines.end();) {
        RenderPipelineCacheEntry const& entry{ iterator.value() };
        if (mFrameCount >
                (entry.lastUsedFrameCount + F_WEBGPU_RENDER_PIPELINE_EXPIRATION_IN_FRAME_COUNT)) {
            // pipeline expired...
            iterator = mRenderPipelines.erase(iterator);
        } else {
            // pipeline not yet expired...
            ++iterator;
        }
    }
}

}  //namespace filament::backend
