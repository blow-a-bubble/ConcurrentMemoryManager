#define  _CRT_SECURE_NO_WARNINGS
#include "GlobalCache.h"
#include "PageManager.h"


GlobalCache* GlobalCache::GetInstance()
{
	static GlobalCache instance;
	return &instance;
}

// 获取⼀个⾮空的span
Span* GlobalCache::GetOneSpan(SpanList& list, size_t size)
{
	//先检查当前GlobalCache中有没有可用的Span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->freeList_ != nullptr)
		{
			return it;
		}
		else
		{
			it = it->next_;
		}
	}

	//走到这里，表示当前GlobalCache中没有可以使用的Span
	//去PageManager中获取
	//计算要获取管理几页的span
	size_t k = SizeClass::NumMovePage(size);
	//解开GlobalCache的桶锁，因为此时尽管当前桶中暂时没有可用的span，但是localcache可以归还内存
	list.mtx_.unlock();
	
	PageManager::GetInstance()->mtx_.lock();
	Span* span = PageManager::GetInstance()->NewSpan(k);
	
	span->objectSize_ = size;
	span->isUse_ = true;
	PageManager::GetInstance()->mtx_.unlock();

	//走到这里，只有当前线程能拿到对应的span进行切分，切分期间不用加锁，因为别的线程不可能访问的到
	//将获取到的span切分成小块内存，挂到对应的哈希桶list中
	char* begin = (char*)(span->id_ << PAGE_SHIFT);
	size_t bytes = span->n_ << PAGE_SHIFT;
	char* end = begin + bytes;
	span->freeList_ = begin;
	char* tail = begin;
	char* cur = begin + size;
	//这里采用尾插法，提高缓存利用率
	while (cur < end)
	{
		NextObj(tail) = cur;
		tail = cur;
		NextObj(tail) = nullptr;
		cur += size;
	}//
	//将切好的span挂到GlobalCache对应的哈希桶
	//走到这里，需要访问GlobalCache对应的哈希桶，在这里重新加锁
	list.mtx_.lock();
	list.PushFront(span);
	return span;
}

// 从中⼼缓存获取⼀定数量的对象给local cache
size_t GlobalCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	assert(batchNum > 0);
	assert(size > 0);

	size_t index = SizeClass::Index(size);
	//加桶锁
	spanLists_[index].mtx_.lock();
	Span* span = GetOneSpan(spanLists_[index], size);
	assert(span);
	assert(span->freeList_);
	
	//从span->freeList_中获取batchNum个小内存块
	size_t actualNum = 1;
	start = end = span->freeList_;
	while (actualNum < batchNum && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		actualNum++;
	}
	//将自由链表中的[start, end]移走，并且end->next = nullptr
	span->freeList_ = NextObj(end);
	NextObj(end) = nullptr;
	span->use_count_ += actualNum;
	spanLists_[index].mtx_.unlock();
	return actualNum;
}



// 将⼀定数量的对象释放到span跨度
void GlobalCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	spanLists_[index].mtx_.lock();
	while (start)
	{
		void* next = NextObj(start);
		Span* span = PageManager::GetInstance()->MapObjectToSpan(start);
		//头插入span
		NextObj(start) = span->freeList_;
		span->freeList_ = start;
		span->use_count_--;
		//如果所有的小内存块都还回来了
		if (span->use_count_ == 0)
		{
			spanLists_[index].Erase(span);
			span->freeList_ = nullptr;
			span->next_ = nullptr;
			span->prev_ = nullptr;
			span->objectSize_ = 0;
			//此时可以暂时解锁了，因为span已经从GlobalCache中拿出来了，别的线程访问不到
			//解锁了，让其他线程可以进行释放或者申请内存
			spanLists_[index].mtx_.unlock();
			PageManager::GetInstance()->mtx_.lock();
			PageManager::GetInstance()->ReleaseSpanToPageManager(span);
			PageManager::GetInstance()->mtx_.unlock();
			spanLists_[index].mtx_.lock();  
		}
		
		start = next;
	}


	spanLists_[index].mtx_.unlock();

}

