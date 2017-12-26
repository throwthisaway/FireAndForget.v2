#pragma once
#undef max
using BufferIndex = size_t;	// index to a vector of GPU buffers
const BufferIndex InvalidBufferIndex = std::numeric_limits<size_t>::max();
using ShaderResourceIndex = size_t; // global index to a shader reosurce in a resource heap
const ShaderResourceIndex InvalidShaderResourceIndex = std::numeric_limits<size_t>::max();
using ResourceHeapHandle = size_t;	// handle of a resource heap/allocator
const ResourceHeapHandle InvalidResourceHeap = std::numeric_limits<size_t>::max();
