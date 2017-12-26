#pragma once
#include <vector>
#include "compatibility.h"
#include "Img.h"
#include "RendererTypes.h"
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
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes, size_t elementSize);
	BufferIndex CreateTexture(const void* buffer, uint64_t width, uint32_t height, uint8_t bytesPerPixel, Img::PixelFormat format);
	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, ResourceHeapHandle shaderResourceHeap, const std::vector<size_t>& vsBuffers, const std::vector<size_t>& psBuffers, const Mesh&);
	void BeginUploadResources();
	void EndUploadResources();
	uint32_t GetCurrenFrameIndex();

	ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);

	static const int frameCount_;
private:
#ifdef PLATFORM_MAC_OS
	void* renderer;
#elif defined(PLATFORM_WIN)
	Renderer* renderer;
#endif
};
