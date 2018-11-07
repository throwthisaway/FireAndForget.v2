#pragma once
#include <vector>
#import <Metal/MTLBuffer.h>
#import <Metal/MTLDevice.h>

class CBFrameAlloc {
	id<MTLDevice> device_;
	std::vector<id<MTLBuffer>> pool_;
	uint32_t index_;
	uint64_t max_, offset_;
	void Request();
public:
	void Init(id<MTLDevice> device, uint64_t bufferSize);
	struct Entry {
		id<MTLBuffer> buffer;
		uint8_t* address;
		uint64_t offset;
		uint32_t size;
	};
	Entry Alloc(uint32_t size);
	void Reset();
};

