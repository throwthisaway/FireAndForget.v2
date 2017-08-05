#ifndef RendererInterface_h
#define RendererInterface_h
#include <vector>

struct MeshDesc {
	std::vector<size_t> buffers;
	size_t offset, count;
};

class RendererWrapper {
public:
	void Init(void* self);
	size_t CreateBuffer(const void* buffer, size_t length, size_t elementSize);
	void SubmitToPipeline(size_t index, uint8_t* uniforms, MeshDesc&&);
private:
	void* self;
};
#endif /* RendererInterface_h */
