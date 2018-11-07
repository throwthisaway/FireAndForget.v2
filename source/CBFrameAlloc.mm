#include "CBFrameAlloc.hpp"
#include "cpp/BufferUtils.h"

void CBFrameAlloc::Request() {
	id<MTLBuffer> buffer = [device_ newBufferWithLength: (NSUInteger)max_ options: MTLResourceCPUCacheModeWriteCombined];
	// TODO:: [buffer setName: (std::wstring(L"frame alloc. constant buffer #") + std::to_wstring(pool_.size())).c_str()))]
	pool_.push_back(buffer);
	index_ = uint32_t(pool_.size() - 1);
	offset_ = 0;
}
void CBFrameAlloc::Init(id<MTLDevice> device, uint64_t bufferSize) {
	offset_ = 0;
	device_ = device;
	max_ = bufferSize;
	pool_.clear();
	Request();
}

CBFrameAlloc::Entry CBFrameAlloc::Alloc(uint32_t size) {
	size = AlignTo<decltype(size), 256>(size);
	if (offset_ + size > max_ && ++index_ >= pool_.size()) Request();
	id<MTLBuffer> buffer = pool_[index_];
	uint8_t* contents = (uint8_t*)[buffer contents];
	Entry result = { buffer, contents + offset_, offset_, size};
	offset_ += size;
	return result;
}
void CBFrameAlloc::Reset() {
	index_ = 0;
	offset_ = 0;
}
