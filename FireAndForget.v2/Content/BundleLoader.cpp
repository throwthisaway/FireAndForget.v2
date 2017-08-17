#include "pch.h"
#include "BundleLoader.h"
#include <MeshLoader.h>

using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::ApplicationModel;
using namespace Platform;
using namespace concurrency;

namespace {
	void LoadChunkHeader(DataReader^ dataReader, MeshLoader::Mesh& mesh, std::function<void(bool)> completionCb) {
		create_task(dataReader->LoadAsync(sizeof(MeshLoader::Chunk))).then([=, &mesh](task<unsigned int> bytesLoaded) {
			if (bytesLoaded.get() < sizeof(MeshLoader::Chunk)) {
				completionCb(true);
				return;
			}
			auto data = ref new Array<byte>(bytesLoaded.get());
			dataReader->ReadBytes(data);
			MeshLoader::Chunk chunk;
			memcpy(&chunk, data->Data, data->Length);
			create_task(dataReader->LoadAsync(chunk.size)).then([=, &mesh](task<unsigned int> bytesLoaded) mutable {
				if (bytesLoaded.get() < sizeof(MeshLoader::Chunk)) {
					completionCb(false);
					return;
				}
				auto data = ref new Array<byte>(bytesLoaded.get());
				dataReader->ReadBytes(data);
				// pass ownership of raw ptr
				uint8_t* raw = new uint8_t[data->Length];
				memcpy(raw, data->Data, data->Length);
				MeshLoader::LoadChunk(chunk.tag, chunk.size, chunk.count, raw, mesh);
				LoadChunkHeader(dataReader, mesh, completionCb);
			});
		});
	}
}
task<void> LoadFromBundle(Platform::String^ fname, MeshLoader::Mesh& mesh, std::function<void(bool)> completionCb) {
	//auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
	//return create_task(folder->GetFilesAsync()).then([](Windows::Foundation::Collections::IVectorView<StorageFile^>^ vec) {
	//	for (size_t i = 0; i < vec->Size; ++i) {
	//		auto name = vec->GetAt(i)->DisplayName;
	//		int j = 0;
	//	}
	//	
	//});
	return create_task(Package::Current->InstalledLocation->GetFileAsync(fname)).then([](StorageFile^ file) {
		return file->OpenSequentialReadAsync();
	}).then([=, &mesh](task<IInputStream ^> inputStream) mutable {
		DataReader^ dataReader = ref new DataReader(inputStream.get());
		LoadChunkHeader(dataReader, mesh, completionCb);
	});
}
