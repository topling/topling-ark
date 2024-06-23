/* vim: set tabstop=4 : */
#ifndef MruCache_h__
#define MruCache_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/stdtypes.hpp>
#include "thread/lockable.hpp"
#include "thread/LockSentry.hpp"
//#include "thread/LockableContainer.hpp"
#include "io/DataIO.hpp"

#include <boost/tuple/tuple.hpp>

/*
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>
*/
namespace terark {

/**
 @name 使用 boost::multi_index 时，用作 index 标识
 @{
 */
struct TagID{};
struct TagKey{};
struct TagName{};
struct TagTime{};
struct TagMru{};
//@}

/** @defgroup MruCache 使用最近最少使用置换算法管理缓存
 @{
 @see MruCache

 @par 用一个例子来说明用法【bbs 热帖分析中的频谱缓存】
 @code
	using namespace boost::multi_index;
	using terark::TagKey;
	using terark::TagMru;

	struct Spectrum : public nlpp::Spectrum
	{
		Spectrum(uint64_t docID = 0) : docID(docID), hitCount(0) {}

		uint64_t docID;
		uint32_t hitCount;

		template<class Output> void save(Output& output) const
		{
			Spectrum& self = const_cast<Spectrum&>(*this);
			output & docID & hitCount;
			uint32_t dataSize = self.getdatasize();
			output & dataSize;
			if (output.write(self.getdata(), dataSize) != dataSize)
				throw terark::OutOfSpaceException();
		}
		template<class Input> void load(Input& input)
		{
			uint32_t dataSize;
			input & docID & hitCount & dataSize;
			terark::DataBufferPtr data(dataSize);

			if (input.read(data->data(), dataSize) != dataSize)
				throw terark::EndOfFileException();

			this->set(dataSize, data->data());
		}
		DATA_IO_REG_LOAD_SAVE(Spectrum)
	};

	class SpectrumCache;
	typedef terark::MruCache<multi_index_container<
		Spectrum,
		indexed_by<
			ordered_unique<tag<TagKey>, member<Spectrum, uint64_t, &Spectrum::docID> >,
			sequenced<tag<TagMru> >
		> >,
		SpectrumCache>
	SpectrumCacheBase;

	class SpectrumCache : public SpectrumCacheBase
	{
		CorrelationAnaylysis* owner;
	public:
		Spectrum makeValue(const boost::tuples::tuple<const uint64_t&>& param);
		bool onCacheHit(Spectrum& x) { x.hitCount++; return true; } //!< optional
		bool onSwapOut(Spectrum& x)  { return true; } //!< optional

		SpectrumCache(CorrelationAnaylysis* owner) : owner(owner) {}

		void print()
		{
			if (size())
			{
				printf("SpectrumCache[size=%u, hit=%llu, swapout=%llu, hit/cache=%llu%%]\n",
					size(),
		   			hitCount(),
		   			swapOutCount(),
		   			hitCount()*100/(hitCount() + size() + swapOutCount())
				);
			}
		}
		DATA_IO_REG_LOAD_SAVE(SpectrumCache)
	};

	Spectrum SpectrumCache::makeValue(const boost::tuples::tuple<const uint64_t&>& param)
	{
		const uint64_t docID = param.get<0>();

		DocTextCache::const_iterator iter = owner->docTextCache.cache(docID);
		Spectrum spec(docID);

		std::wstring wtext;
		nlpp::str2wstr((*iter).text, wtext);
		owner->freqCmp.getfreq(wtext, spec);

		return spec;
	}
 @endcode
 @}
 */

/**
 @brief MruCache 的基础实现类，仅由内部使用，不能由用户代码直接使用

 @param MultiIndex 用户定义的 boost::multi_index 容器
 @param SelfReflected requires member:
  - makeValue[optional]:
	- value_type makeValue(const boost::tuples::tuple<const mru_key_type&>& p);
	- value_type makeValue(const boost::tuples::tuple<const mru_key_type&, const T1&>& p);
	- value_type makeValue(const boost::tuples::tuple<const mru_key_type&, const T1&, const T2&>& p);
  - bool onCacheHit(value_type&) // optional
  - bool onSwapOut(value_type&)  // optional
  - bool onTimeout(value_type&, time_t curTime) // optional
 */
template<class MultiIndex, class SelfReflected>
class MruCacheBaseImpl : public MultiIndex
{
// maybe copyable!!
	typedef MultiIndex super;
public:
	typedef typename super::value_type value_type;
	typedef typename super::reference  reference;
	typedef typename super::iterator   iterator;

protected:
	/**
	 @brief 禁止用户直接构造本类对象
	 */
	MruCacheBaseImpl() : m_hitCount(0), m_swapOutCount(0) {}

protected:
	size_t	 m_maxSize;		  //!< this member initialized by class MruCache
	uint64_t m_hitCount;	  //!< 击中次数
	uint64_t m_swapOutCount;  //!< 换出的对象总数

	SelfReflected& self() { return static_cast<SelfReflected&>(*this); }

	typedef typename super::template index<TagMru>::type MruIndex;
	typedef typename super::template index<TagKey>::type KeyIndex;
	typedef typename KeyIndex::key_type mru_key_type;

	KeyIndex& keyIndex() throw() { return super::template get<TagKey>(); }
	MruIndex& mruIndex() throw() { return super::template get<TagMru>(); }
	const KeyIndex& keyIndex() const throw() { return super::template get<TagKey>(); }
	const MruIndex& mruIndex() const throw() { return super::template get<TagMru>(); }

	template<class OtherIndexIter>
	iterator toMainIter(OtherIndexIter iter) throw()
	{
		return super::template project<0>(iter);
	}
///////////////////////////////////////////////////////////////////////////////////
protected:
	//! only called by timeoutScan()...
	void erase_element(typename MruIndex::iterator iter) { mruIndex().erase(iter); }

	//! called by cache
	template<class T0, class T1, class T2, class T3, class T4,
			 class T5, class T6, class T7, class T8, class T9>
	iterator cache_Impl(const boost::tuples::tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9>& p)
	{
		return cache_Impl_2(p);
	}

	template<class Tuple> iterator cache_Impl_2(const Tuple& p);

//protected:
public:
	//@{
	//! default onChacheHit/onSwapOut...
	bool onCacheHit(reference x) { return true; }
	bool onSwapOut (reference x) { return true; }
	bool onTimeout (reference x, time_t curTime) { return true; }
	//@}

	//@{
	/** 创建新对象[相当于根据对象的键将它载入cache]

		default is to use constructor to makeValue...\n
		but derived class 'SelfReflected' can specialize other implement,\n
		such as for pointer to new an object ptr by parameters..
	 */
	value_type makeValue(const boost::tuples::tuple<const mru_key_type&>& p)
	{
		return value_type(p.template get<0>());
	}
	template<class T1>
	value_type makeValue(const boost::tuples::tuple<const mru_key_type&, const T1&>& p)
	{
		return value_type(p.template get<0>(),
						  p.template get<1>());
	}
	template<class T1, class T2>
	value_type makeValue(const boost::tuples::tuple<const mru_key_type&, const T1&, const T2&>& p)
	{
		return value_type(p.template get<0>(),
						  p.template get<1>(),
						  p.template get<2>());
	}
	//@}

public:
	void setMaxSize(int maxSize)
	{
		while (int(this->size()) > maxSize)
		{
		// 加上这个条件判断有可能导致死循环，当有一个不满足条件时，就陷入死循环
		//	if (self().onSwapOut(const_cast<value_type&>(mruIndex().back())))
		//	{
		//		mruIndex().pop_back();
		//	}

		// 强制换出，不管 SwapOut 是否成功，都将尾元素删除
			self().onSwapOut(const_cast<value_type&>(mruIndex().back()));
			mruIndex().pop_back();
		}
		m_maxSize = maxSize;
	}
	int maxSize() const { return m_maxSize; }

	uint64_t hitCount() const { return m_hitCount; }
	uint64_t swapOutCount() const { return m_swapOutCount; }

	void hit(typename MruIndex::iterator iter) {
		assert(mruIndex().end() != iter);
		m_hitCount++;
		mruIndex().relocate(mruIndex().begin(), iter);
	}
	void hit(typename KeyIndex::iterator iter) {
		assert(keyIndex().end() != iter);
		m_hitCount++;
		mruIndex().relocate(mruIndex().begin(),
			super::template project<TagMru>(iter));
	}
	template<class OtherIndexIterator>
	void hit(OtherIndexIterator iter) {
		// has no assert(iter....)!!
		m_hitCount++;
		mruIndex().relocate(mruIndex().begin(),
			super::template project<TagMru>(iter));
	}

	//@{
	/**
	 @brief 获取/缓存对象

	  如果对象不在 cache 中，则调用 makeValue 载入它
	 */
	iterator cache(const value_type& val);

	iterator cache(const mru_key_type& key)
	{
		return cache_Impl(boost::make_tuple(key));
	}
	template<class T1>
	iterator cache(const mru_key_type& key, const T1& p1)
	{
		return cache_Impl(boost::make_tuple(key, p1));
	}
	template<class T1, class T2>
	iterator cache(const mru_key_type& key, const T1& p1, const T2& p2)
	{
		return cache_Impl(boost::make_tuple(key, p1, p2));
	}
	template<class T1, class T2, class T3>
	iterator cache(const mru_key_type& key, const T1& p1, const T2& p2, const T3& p3)
	{
		return cache_Impl(boost::make_tuple(key, p1, p2, p3));
	}
	//@}

	/**
	 @brief 仅获取对象

	  如果对象不在 cache 中，则返回 this->end()
	 */
	iterator take(const mru_key_type& key)
	{
		typename KeyIndex::iterator iter = keyIndex().find(key);
		if (keyIndex().end() != iter &&
				self().onCacheHit(const_cast<reference>(*iter)))
	   	{ // hited success, put it into front
			hit(iter);
		}
		return toMainIter(iter);
	}

	/**
	 @brief 通过对象的键来删除对象

	  不管对象在不在 cache 中，调用该函数之后，确保键值为 key 的对象已不在 cache 中
	 */
	void remove(const mru_key_type& key)
	{
		typename KeyIndex::iterator	iter = keyIndex().find(key);
		if (keyIndex().end() != iter)
			super::erase(toMainIter(iter));
	}

	/**
	 @brief 测试键值为 key 的对象是否存在
	 */
	bool exist(const mru_key_type& key)
	{
		return keyIndex().find(key) != keyIndex().end();
	}

	/**
	 @brief 超时扫描

	  如果用户希望超时扫描，可以周期性地调用该函数，该函数会将缓存中超时的对象删除
	 */
	void timeoutScan();
};

template<class MultiIndex, class SelfReflected>
template<class Tuple> inline
typename MruCacheBaseImpl<MultiIndex, SelfReflected>::iterator
MruCacheBaseImpl<MultiIndex, SelfReflected>::cache_Impl_2(const Tuple& p)
{
	typename KeyIndex::iterator iter = keyIndex().find(p.template get<0>());
	if (keyIndex().end() == iter)
	{
		if (super::size() >= m_maxSize) {
			// super maybe lockable,
			//   only can use 'erase' to erase element, it maybe lockable..
			// if super is LockableContainer,
			//   'erase' will wait for the oldest element,
			//   see LockableContainer::erase()
			//
			typename MruIndex::iterator last = mruIndex().end();
			--last;
			if (self().onSwapOut(const_cast<reference>(*last))) {
				m_swapOutCount++;
				self().erase_element(last);
			}
		}
		std::pair<typename MruIndex::iterator, bool> head;
		head = mruIndex().push_front(self().makeValue(p));
		return toMainIter(head.first);
	}
	if (self().onCacheHit(const_cast<reference>(*iter))) {
		// put in front
		hit(iter);
	}
	return toMainIter(iter);
}

template<class MultiIndex, class SelfReflected> inline
typename MruCacheBaseImpl<MultiIndex, SelfReflected>::iterator
MruCacheBaseImpl<MultiIndex, SelfReflected>::cache(const value_type& val)
{
	typename KeyIndex::iterator
		iter = keyIndex().find(keyIndex().key_extractor()(val));
	if (keyIndex().end() == iter)
	{
		if (super::size() >= m_maxSize) {
			// super maybe lockable,
			//   only can use 'erase' to erase element, it maybe lockable..
			// if super is LockableContainer,
			//   'erase' will wait for the oldest element,
			//   see LockableContainer::erase()
			//
			typename MruIndex::iterator last = mruIndex().end();
			--last;
			if (self().onSwapOut(const_cast<reference>(*last))) {
				m_swapOutCount++;
				self().erase_element(last);
			}
		}
		std::pair<typename MruIndex::iterator, bool> head;
		head = mruIndex().push_front(val); // call value_type's copy constructor

		return toMainIter(head.first);
	}
	if (self().onCacheHit(const_cast<reference>(*iter))) {
		// put in front
		hit(iter);
	}
	return toMainIter(iter);
}

template<class MultiIndex, class SelfReflected> inline
void MruCacheBaseImpl<MultiIndex, SelfReflected>::timeoutScan()
{
	time_t curTime = ::time(0);
	typename MruIndex::iterator	iter = mruIndex().end();
	if (iter != mruIndex().begin()) do { --iter;
		if (self().expireTime(*iter) < curTime) {
			if (self().onTimeout(const_cast<reference>(*iter), curTime))
				// erase expired...
				iter = mruIndex().erase(iter);
		} else {
			// reach the oldest item,
			// if it is not expired, others are not expired also.
			break;
		}
	} while (iter != mruIndex().begin());
	return;
}

/////////////////////////////////////////////////////////////////////////////////////

/**
 @brief MruCache 的基类，用于类型推导

  通过 LockableTag 来断定用户提供的 MultiIndex 拥有何种锁定类型，自动选择正确的实现
  默认的是不可锁定类型
 */
template<class MultiIndex, class SelfReflected, class LockableTag>
class MruCacheBase :
   	public MruCacheBaseImpl<MultiIndex, SelfReflected>
{
	// maybe copyable!!
protected:
	MruCacheBase() {}
};

/**
 @brief MultiIndex 是可互斥锁的[MutexLockableTag]
 */
template<class MultiIndex, class SelfReflected>
class MruCacheBase<MultiIndex,
	 			   SelfReflected,
				   thread::MutexLockableTag> :
	public MruCacheBaseImpl<MultiIndex, SelfReflected>
{
	DECLARE_NONE_COPYABLE_CLASS(MruCacheBase)

	friend class MruCacheBaseImpl<MultiIndex, SelfReflected>;
	typedef 	 MruCacheBaseImpl<MultiIndex, SelfReflected> super;

protected:
	MruCacheBase() {}

public:
	//! cache into '*this' with lock, do not return the iterator
	//! because after unlocked, and lock again, the iterator maybe has erase by another thread!
	void lockedCache(const typename super::value_type& val)
	{
		typename super::MutexLockSentry sentry(*this);
		super::cache(val);
	}
	bool exist(const typename super::mru_key_type& key)
	{
		typename super::MutexLockSentry sentry(*this);
		return super::exist(key);
	}
	void remove(const typename super::mru_key_type& key)
	{
		typename super::MutexLockSentry sentry(*this);
		super::remove(key);
	}
	void timeoutScan()
	{
		// maybe wait for get the write lock...
		typename super::MutexLockSentry sentry(*this);
		super::timeoutScan();
	}
};

/**
 @brief MultiIndex 是可读写锁的[ReadWriteLockableTag]
 */
template<class MultiIndex, class SelfReflected>
class MruCacheBase<MultiIndex,
	 			   SelfReflected,
				   thread::ReadWriteLockableTag> :
	public MruCacheBaseImpl<MultiIndex, SelfReflected>
{
	DECLARE_NONE_COPYABLE_CLASS(MruCacheBase)

	friend class MruCacheBaseImpl<MultiIndex, SelfReflected>;
	typedef 	 MruCacheBaseImpl<MultiIndex, SelfReflected> super;

protected:
	MruCacheBase() {}

public:
	// cache into '*this' with lock, do not return the iterator
	// because after unlocked, and lock again, the iterator maybe has erase by another thread!
	void lockedCache(const typename super::value_type& val)
	{
		typename super::WriteLockSentry sentry(*this);
		super::cache(val);
	}
	bool exist(const typename super::mru_key_type& key)
	{
		typename super::ReadLockSentry sentry(*this);
		return super::exist(key);
	}
	void remove(const typename super::mru_key_type& key)
	{
		typename super::WriteLockSentry sentry(*this);
		super::remove(key);
	}
	void timeoutScan()
	{
		// maybe wait for get the write lock...
		typename super::WriteLockSentry sentry(*this);
		super::timeoutScan();
	}
};

/**
 @brief MultiIndex 是可锁定容器中元素的[ContainerElementLockableTag]

 一般是 LockableContainer
 */
template<class MultiIndex, class SelfReflected>
class MruCacheBase<MultiIndex,
	 			   SelfReflected,
				   thread::ContainerElementLockableTag> :
	public MruCacheBaseImpl<MultiIndex, SelfReflected>
{
	DECLARE_NONE_COPYABLE_CLASS(MruCacheBase)

	friend class MruCacheBaseImpl<MultiIndex, SelfReflected>;
	typedef 	 MruCacheBaseImpl<MultiIndex, SelfReflected> super;

protected:
	typedef typename super::KeyIndex KeyIndex;
	typedef typename super::MruIndex MruIndex;
	using super::keyIndex;
	using super::mruIndex;

public:
	typedef typename super::value_type value_type;
	typedef typename super::reference  reference;
	typedef typename super::iterator   iterator;
	typedef typename super::ElemRefPtr ElemRefPtr;
	typedef typename super::mru_key_type mru_key_type;

protected:
	MruCacheBase() {}

	void erase_element(typename MruIndex::iterator iter)
	{ // erase will successful sure!!!
		if (super::isLockedElement(*iter)) {
		//	super::erase(iter);
		} else {
			mruIndex().erase(iter);
		}
	}

public:
	/**
	 @brief 带锁的获取

	 @return if success, bool(elemRef) is true!!
	 @note do not lock 'this' before call this function
	 */
	ElemRefPtr lockedTake(const mru_key_type& key)
	{
		typename super::WriteLockSentry sentry(*this);
		iterator iter = super::take(key);
		if (super::end() != iter) {
			return super::getRefPtr(iter);
		}
		return 0;
	}

	//@{
	/**
	 @brief 带锁的获取/缓存

	 @return if success, bool(elemRef) is true!!
	 @note do not lock 'this' before call this function
	 */
	ElemRefPtr lockedCache(const value_type& val)
	{
		typename super::WriteLockSentry sentry(*this);
		return super::getRefPtr(super::cache(val));
	}
	ElemRefPtr lockedCache(const mru_key_type& key)
	{
		typename super::WriteLockSentry sentry(*this);
		return super::getRefPtr(super::cache(key));
	}
	template<class T1>
	ElemRefPtr lockedCache(const mru_key_type& key, const T1& p1)
	{
		typename super::WriteLockSentry sentry(*this);
		return super::getRefPtr(super::cache(key, p1));
	}
	template<class T1, class T2>
	ElemRefPtr lockedCache(const mru_key_type& key, const T1& p1, const T2& p2)
	{
		typename super::WriteLockSentry sentry(*this);
		return super::getRefPtr(super::cache(key, p1, p2));
	}
	//@}

	bool exist(const typename super::mru_key_type& key)
	{
		typename super::ReadLockSentry sentry(*this);
		return super::exist(key);
	}

	/**
	 @brief 删除的特殊实现

	 @note do not lock 'this' before call this function
	 */
	void remove(const mru_key_type& key)
	{
		typename super::WriteLockSentry sentry(*this);
		typename KeyIndex::iterator	iter = super::keyIndex().find(key);
		if (keyIndex().end() != iter)
			// use LockableContainer::erase(iter)....
			super::erase(super::toMainIter(iter));
	}
	/**
	 @brief 尝试删除

	 成功返回 true，失败返回 false

	 @note do not lock 'this' before call this function
	 */
	bool tryRemove(const mru_key_type& key)
	{
		typename super::WriteLockSentry sentry(*this);
		typename KeyIndex::iterator	iter = super::keyIndex().find(key);
		if (keyIndex().end() != iter)
			// use LockableContainer::tryErase(iter)....
			return super::tryErase(super::toMainIter(iter));

		return true; // no elem, return true yet!
	}

	/**
	 @brief 超时扫描的特殊实现

	 @note do not need lock 'this' before calling this method..

	 require time_t expireTime(const_reference);
	 */
	void timeoutScan();
};

template<class MultiIndex, class SelfReflected> inline
void
MruCacheBase<MultiIndex, SelfReflected, thread::ContainerElementLockableTag>::
timeoutScan()
{
	// maybe wait for get write lock...
	typename super::WriteLockSentry sentryThis(*this);

	// curTime must is after gotten lock!!
	time_t curTime = ::time(0);

	typename MruIndex::iterator iter = mruIndex().end();
	if (iter != mruIndex().begin()) do { --iter;
		if (super::self().expireTime(*iter) < curTime) {
			if (super::isLockedElement(*iter)) {
				// skip locked element....
				// can not erase right now, only can eraseOnRelease instead!
				// super::eraseOnRelease(*iter);
			} else if (super::self().onTimeout(const_cast<reference>(*iter), curTime))
				// erase expired, iter point to 'next' pos of current,
				// at the first of next cycle, iter will be decreased...
				iter = mruIndex().erase(iter);
			else {
				// do nothing...
			}
		} else {
			// reach the oldest item,
			// if it is not expired, others are not expired also.
			break;
		}
	} while (iter != mruIndex().begin());

	return;
}

/**
 @ingroup MruCache
 @brief 提供给用户的 MruCache 接口
 */
template<class MultiIndex, class SelfReflected>
class MruCache :
   	public MruCacheBase<
		MultiIndex,
	   	SelfReflected,
		typename thread::LockableTraits<MultiIndex>::LockableTag
		>
{
	//! maybe copyable!!
   	typedef MruCacheBase<
		MultiIndex,
	   	SelfReflected,
		typename thread::LockableTraits<MultiIndex>::LockableTag
		>
	super;
public:
	typedef typename thread::LockableTraits<MultiIndex>::LockableTag lockable_tag;

	MruCache(size_t maxSize = 10) { this->m_maxSize = maxSize; }

	typedef typename super::value_type value_type;
	typedef typename super::MruIndex MruIndex;
	typedef typename super::KeyIndex KeyIndex;
	typedef typename KeyIndex::key_type   mru_key_type;

	BOOST_STATIC_CONSTANT(unsigned int, class_version = 1);
	//@{
	/**
	 @brief 序列化支持

	 @note 假定在序列化时是单线程的，调用者必须保证这一点
	 */
	template<class Output> void save(Output& output, unsigned int version) const
	{
		uint32_t size = super::size();
		uint32_t maxSize = this->m_maxSize;
		output & size & maxSize & this->m_hitCount & this->m_swapOutCount;
		for (typename MruIndex::const_iterator iter = super::mruIndex().begin();
			   	iter != super::mruIndex().end(); ++iter)
		{
			output << *iter;
		}
	}
	template<class Input> void load(Input& input, unsigned int version)
	{
		uint32_t size, maxSize;
		input & size & maxSize & this->m_hitCount & this->m_swapOutCount;
		this->m_maxSize = maxSize;
		for (uint32_t i = 0; i < size; ++i)
		{
			value_type val;
			input >> val;
			super::mruIndex().push_back(val);
		}
	}
	DATA_IO_REG_LOAD_SAVE_V(MruCache, class_version)
	//@}
};

} // namespace terark

#endif // MruCache_h__

