#pragma once
#include "ShaderStructures.h"
#include "RendererTypes.h"
#include "RendererWrapper.h"

ShaderResourceIndex CreateShaderParam(RendererWrapper* renderer, ResourceHeapHandle heapHandle, size_t size) {
	return renderer->GetShaderResourceIndex(heapHandle, size, RendererWrapper::frameCount_);
}

template<typename... T>
std::initializer_list<ShaderResourceIndex> CreateShaderParams(RendererWrapper* renderer, ResourceHeapHandle heapHandle) {
	return {CreateShaderParam(renderer, heapHandle, sizeof(typename T::types))...};
}
