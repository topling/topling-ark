#include "toast.hpp"
#include "persistent_alloc.hpp"
#include <terark/io/MemMapStream.hpp>
#include <db_cxx.h>

namespace terark {

class TERARK_DLL_EXPORT MMS_HugeChunkAlloc : public PersistentAllocator_LL::hc_alloc_t
{
	MemMapStream* mms;
public:
	MMS_HugeChunkAlloc(MemMapStream* mms)
	{
		this->mms = mms;
	}
	virtual std::pair<stream_position_t,uint32_t> chunk_alloc(uint32_t size)
	{
		std::pair<stream_position_t,uint32_t> ps;
		ps.first = mms->size();
		ps.second = align_up(size, 64*1024);
		mms->set_fsize(ps.first + ps.second);
		return ps;
	}
};

class ToastBlob_impl
{
public:
	MemMapStream mms;
	MMS_HugeChunkAlloc hugeChunkAlloc;
	PersistentAllocator_LL alloc;

	ToastBlob_impl(DbEnv* env,
					const std::string& prefix,
					const std::string& toastFile,
					uint32_t cell_size,
					uint32_t initial_size)
	  : mms(0, toastFile, O_CREAT|O_RDWR)
	  , hugeChunkAlloc(&mms)
	  , alloc(env, prefix, cell_size, initial_size, &hugeChunkAlloc)
	{
	}
};

ToastBlob::ToastBlob(DbEnv* env,
		const std::string& prefix,
		const std::string& toastFile,
		uint32_t cell_size,
		uint32_t initial_size)
	: impl(new ToastBlob_impl(env, prefix, toastFile, cell_size, initial_size))
{
}

ToastBlob::~ToastBlob()
{
	delete impl;
}

stream_position_t ToastBlob::insert(const void* data, uint32_t size)
{
	stream_position_t pos = impl->alloc.alloc(size);
	impl->mms.seek(pos);
	impl->mms.ensureWrite(data, size);
	return pos;
}

stream_position_t ToastBlob::update(stream_position_t old_pos, uint32_t old_size, const void* data, uint32_t* size)
{
	uint32_t size0 = *size;
	stream_position_t pos = impl->alloc.alloc(old_pos, old_size, size0, size);
	impl->mms.seek(pos);
	impl->mms.ensureWrite(data, size0);
	return pos;
}

stream_position_t ToastBlob::append(stream_position_t old_pos, uint32_t old_size, const void* data, uint32_t size)
{
	uint32_t total = old_size + size;
	stream_position_t pos = impl->alloc.alloc(old_pos, old_size, total, &total);
	if (old_pos == pos) {
		impl->mms.seek(pos + old_size);
		impl->mms.ensureWrite(data, size);
	} else {
		MMS_MapData map(impl->mms, old_pos, old_size);
		impl->mms.seek(pos);
		impl->mms.ensureWrite(map.data(), old_size);
		impl->mms.ensureWrite(data, size);
	}
	return impl->mms.tell();
}

void ToastBlob::query(stream_position_t pos, void* data, uint32_t size)
{
	MMS_MapData map(impl->mms, pos, size);
	memcpy(data, map.data(), size);
}

} // namespace terark

