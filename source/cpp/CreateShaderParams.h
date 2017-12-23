#pragma once
#include "ShaderStructures.h"
#include "RendererTypes.h"
#include "RendererWrapper.h"

template<typename T>
ShaderResourceIndex ExtractShaderParam(RendererWrapper* renderer, ResourceHeapHandle heapHandle) {
	return renderer->GetShaderResourceIndex(heapHandle, sizeof(T), DX::c_frameCount);
}

template<typename T0, typename... TRest>
std::initializer_list<ShaderResourceIndex> ExtractShaderParam<T0, TRest...>(RendererWrapper* renderer, ResourceHeapHandle heapHandle) {
	return { ExtractShaderParam<T0>(heapHandle, heapHandle), ExtractShaderParam<TRest...>(renderer, heapHandle) };
}

template<typename T>
std::initializer_list<ShaderResourceIndex> CreateShaderParams<T>(RendererWrapper* renderer, ResourceHeapHandle heapHandle) {
	return ExtractShaderParam<T::types...>(renderer, heapHandle);
}
