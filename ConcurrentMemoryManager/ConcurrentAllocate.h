#pragma once
#include "LocalCache.h"
#include "PageManager.h"
#include "ObjectPool.h"

static void* ConcurrentAllocate(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t roundUpSize = SizeClass::RoundUp(size);
		size_t kPage = roundUpSize >> PAGE_SHIFT;
		PageManager::GetInstance()->mtx_.lock();
		Span* span = PageManager::GetInstance()->NewSpan(kPage);
		span->objectSize_ = kPage << PAGE_SHIFT;
		span->isUse_ = true;
		PageManager::GetInstance()->mtx_.unlock();
		void* ret = reinterpret_cast<void*>(span->id_ << PAGE_SHIFT);
		return ret;
	}
	else
	{
		if (pLocalCache == nullptr)
		{
			static ObjectPool<LocalCache> threadPool;
			pLocalCache = threadPool.New();
		}
		//cout << std::this_thread::get_id() << ":" << pLocalCache << endl;
		return pLocalCache->Allocate(size);
	}
	
}

static void ConcurrentDeAllocate(void* obj)
{
	Span* span = PageManager::GetInstance()->MapObjectToSpan(obj);
	size_t size = span->objectSize_;
	if (size > MAX_BYTES)
	{
		PageManager::GetInstance()->mtx_.lock();
		PageManager::GetInstance()->ReleaseSpanToPageManager(span);
		PageManager::GetInstance()->mtx_.unlock();
	}
	else
	{
		pLocalCache->DeAllocate(obj, size);
	}
}