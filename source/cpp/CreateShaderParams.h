#pragma once
#include "ShaderStructures.h"
#include "RendererTypes.h"
#include "RendererWrapper.h"
//
//ShaderResourceIndex CreateShaderParam(RendererWrapper* renderer, ResourceHeapHandle heapHandle, size_t size) {
//	// TODO:: switch between texture, empty struct as substitution will create a 1 byte buffer...
//	return renderer->GetShaderResourceIndex(heapHandle, size, RendererWrapper::frameCount_);
//}
//
//template<typename... T>
//std::initializer_list<ShaderResourceIndex> CreateShaderParams(RendererWrapper* renderer, ResourceHeapHandle heapHandle) {
//	return {CreateShaderParam(renderer, heapHandle, sizeof(T))...};
//}

//template<template<typename... T> typename TParamTraits>
//constexpr DescriptorIndex DescriptorCount() {
//	return InternalDescriptorCount<T...>();
//}
