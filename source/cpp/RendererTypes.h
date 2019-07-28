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

using MaterialIndex = uint32_t;
const MaterialIndex InvalidMaterial = std::numeric_limits<MaterialIndex>::max();

using ImageIndex = uint32_t;
const ImageIndex InvalidImage = std::numeric_limits<ImageIndex>::max();

using ShaderId = uint16_t;
const ShaderId InvalidShader = std::numeric_limits<ShaderId>::max();

using index_t = uint16_t;	// Index buffer element type
using offset_t = uint32_t;	// byte offset to an opaque array
