#pragma once
#undef max
using BufferIndex = uint32_t;	// index to a vector of GPU buffers
const BufferIndex InvalidBuffer = std::numeric_limits<BufferIndex>::max();

using TextureIndex = uint32_t;	// index to a vector of GPU buffers
const TextureIndex InvalidTexture = std::numeric_limits<TextureIndex>::max();

using ShaderResourceIndex = uint32_t; // index to a shader resource in a cbuffer
const ShaderResourceIndex InvalidShaderResource = std::numeric_limits<ShaderResourceIndex>::max();

using DescAllocEntryIndex = uint32_t;
const DescAllocEntryIndex InvalidDescAllocEntry = std::numeric_limits<DescAllocEntryIndex>::max();
