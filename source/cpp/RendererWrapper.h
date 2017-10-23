#ifndef RendererInterface_h
#define RendererInterface_h
#include <vector>

namespace Materials {
	struct cBuffers;
}

struct Mesh;
struct ShaderResources;

class RendererWrapper {
public:
	void Init(void* self);
	size_t CreateBuffer(const void* buffer, size_t length, size_t elementSize);
	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, const std::vector<size_t>& bufferIndices, const Mesh&);
	void BeginUploadResources();
	void EndUploadResources();
	ShaderResources& GetShaderResources();
	uint32_t GetCurrenFrameIndex();
private:
	void* self;
};
#endif /* RendererInterface_h */
