#ifndef RendererInterface_h
#define RendererInterface_h
#include <vector>

struct Model;

class RendererWrapper {
public:
	void Init(void* self);
	size_t CreateBuffer(const void* buffer, size_t length, size_t elementSize);
	void BeginRender();
	size_t StartRenderPass();
	void SubmitToEncoder(size_t encoderIndex, size_t pipelineIndex, uint8_t* uniforms, const Model&);
	void BeginUploadResources();
	void EndUploadResources();
private:
	void* self;
};
#endif /* RendererInterface_h */
