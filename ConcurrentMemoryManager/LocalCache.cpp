#define  _CRT_SECURE_NO_WARNINGS
#include "LocalCache.h"
#include "GlobalCache.h"
void* LocalCache::Allocate(size_t size)
{
	assert(size > 0 && size < MAX_BYTES);
	void* obj = nullptr;
	size_t roundUpSize = SizeClass::RoundUp(size); //找到申请内存的上界
	size_t index = SizeClass::Index(size); //找到对应的哈希桶下标

	//如果自由链表中有内存
	if (!freeLists_[index].Empty())
	{
		obj = freeLists_[index].Pop();
	}
	//自由链表中没有内存
	else
	{
		obj = FetchFromGlobalCache(index, roundUpSize);
	}
	return obj;
}

void* LocalCache::FetchFromGlobalCache(size_t index, size_t size)
{
	assert(size > 0);
	//慢开始调节反馈算法
	size_t batchNum = min(freeLists_[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batchNum == freeLists_[index].MaxSize())
	{
		freeLists_[index].MaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;
	//获取内存块，返回实际获取的内存块
	size_t actualNum = GlobalCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0); //获取不到函数内部早都抛异常了
	
	//如果只申请到一块内存返回即可
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	//申请到多块内存，第一块内存返回，剩余的挂入LocalCache对应的哈希桶中
	else
	{
		freeLists_[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
	
}

void LocalCache::DeAllocate(void* obj, size_t size)
{
	//直接向对应的自由链表中头插
	size_t index = SizeClass::Index(size);
	freeLists_[index].Push(obj);

	//如果自由链表中的内存块长度超过目前一次向GlobalCache申请的最大长度，
	// 将一次向GlobalCache申请的最大长度还给GlobalCache
	if (freeLists_[index].Size() >= freeLists_[index].MaxSize())
	{
		ListTooLong(freeLists_[index], size);
	}
}

// 释放对象时，链表过⻓时，回收内存回到全局缓存
void LocalCache::ListTooLong(FreeList& list, size_t size)
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	GlobalCache::GetInstance()->ReleaseListToSpans(start, size);
}
