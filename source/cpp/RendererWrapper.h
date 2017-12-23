#pragma once
#include <vector>
#include "Img.h"
#include "RendererTypes.h"

class Renderer;
struct Mesh;

class RendererWrapper {
public:
	void Init(Renderer* self);
	BufferIndex CreateBuffer(const void* buffer, size_t sizeInBytes, size_t elementSize);
	BufferIndex CreateTexture(const void* buffer, UINT width, UINT height, UINT bytesPerPixel, Img::PixelFormat format);
	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, ResourceHeapHandle shaderResourceHeap, const std::vector<size_t>& bufferIndices, const Mesh&);
	void BeginUploadResources();
	void EndUploadResources();
	uint32_t GetCurrenFrameIndex();

	ResourceHeapHandle GetStaticShaderResourceHeap(unsigned short descCountNeeded);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, size_t size, unsigned short count);
	void UpdateShaderResource(ShaderResourceIndex shaderResourceIndex, const void* data, size_t size);
	ShaderResourceIndex GetShaderResourceIndex(ResourceHeapHandle shaderResourceHeap, BufferIndex textureIndex);
private:
	Renderer* renderer;
};
