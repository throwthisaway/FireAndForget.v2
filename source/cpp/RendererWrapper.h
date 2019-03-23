#pragma once
#include <vector>
#include "compatibility.h"
#include "Img.h"
#include "RendererTypes.h"
#include "ShaderStructures.h"
#ifdef PLATFORM_WIN
class Renderer;
#endif
struct Mesh;

class RendererWrapper {
public:
#ifdef PLATFORM_MAC_OS
	void Init(void* self);
#elif defined(PLATFORM_WIN)
	void Init(Renderer* self);
#endif
	void BeginUploadResources();
	BufferIndex CreateBuffer(const void* data, size_t sizeInBytes);
	TextureIndex CreateTexture(const void* data, uint64_t width, uint32_t height, Img::PixelFormat format, const char* label = nullptr);
	TextureIndex GenCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const assets::Submesh& submesh, uint64_t dim, ShaderId shader, bool mip, const char* label = nullptr);
	TextureIndex GenPrefilteredEnvCubeMap(TextureIndex tex, BufferIndex vb, BufferIndex ib, const assets::Submesh& submesh, uint64_t dim, ShaderId shader, const char* label = nullptr);
	TextureIndex GenBRDFLUT(uint64_t dim, ShaderId shader, const char* label = nullptr);
	TextureIndex GenTestCubeMap();
	void EndUploadResources();

	// Shader resources
	ShaderResourceIndex CreateShaderResource(uint32_t size, uint16_t count);
	void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
#ifdef PLATFORM_WIN
	DescAllocEntryIndex AllocDescriptors(uint16_t count);
	void CreateCBV(DescAllocEntryIndex index, uint16_t offset, ShaderResourceIndex resourceIndex);
	void CreateCBV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ShaderResourceIndex resourceIndex);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, BufferIndex textureBufferIndex);
	void CreateSRV(DescAllocEntryIndex index, uint16_t offset, uint32_t frame, ShaderResourceIndex resourceIndex);
#endif
	void BeginRender();
	void StartDeferredPass();
	void StartForwardPass();
	template<typename CmdT>
	void Submit(const CmdT&);

	void SetDeferredBuffers(const ShaderStructures::DeferredBuffers& deferredBuffers);
	uint32_t GetCurrenFrameIndex();

	//ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	//void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	//ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);
private:
#ifdef PLATFORM_MAC_OS
	void* renderer;
#elif defined(PLATFORM_WIN)
	Renderer* renderer;
#endif
};
