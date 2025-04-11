#pragma once
#include "Common.h"

//本质是一个哈希桶，每个哈希桶下挂着自由链表
class LocalCache
{
public:
	//申请内存
	void* Allocate(size_t size);
	//释放内存
	void DeAllocate(void* obj, size_t size);
	//从CentralCache中获取内存
	void* FetchFromGlobalCache(size_t index, size_t size);
	// 释放对象时，链表过⻓时，回收内存回到中⼼缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList freeLists_[N_LISTS]; //哈希桶，挂着自由链表
};

//TLS全局变量，每个线程拥有独立的全局变量,实现无锁控制
static __declspec(thread) LocalCache* pLocalCache = nullptr;