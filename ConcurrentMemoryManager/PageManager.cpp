#define  _CRT_SECURE_NO_WARNINGS
#include "PageManager.h"
PageManager* PageManager::singleInstance_ = nullptr;
std::mutex PageManager::classMtx_;
PageManager* PageManager::GetInstance()
{
	//双检查
	if (singleInstance_ == nullptr)
	{
		std::lock_guard<std::mutex> lg(classMtx_);
		if (singleInstance_ == nullptr)
		{
			singleInstance_ = new PageManager();
		}
	}
	return singleInstance_;
 }

//获取管理k页内存的span
Span* PageManager::NewSpan(size_t k)
{
	assert(k > 0);
	//大于128页的直接向堆申请
	if (k > N_PAGES - 1)
	{
		//Span* span = new Span();
		Span* span = spanPool_.New();
		void* memory = SystemAlloc(k);
		span->id_ = (reinterpret_cast<PAGE_ID>(memory)) >> PAGE_SHIFT;
		span->n_ = k;
		//映射第一页即可，因为这块内存不在哈希桶中，不用合并划分，所以释放的时候肯定是拿第一页的地址来释放
		//idSpanMap_[span->id_] = span;
		idSpanMap_.set(span->id_, span);
		return span;
	}
	//PageCache中k和下表一一对应，0下标空出来不做处理，简化了映射关系
	size_t index = k;
	//如果对应哈希桶中有span
	if (!spanLists_[index].Empty())
	{
		//处理id和span的映射,所有id都要映射
		Span* span = spanLists_[index].PopFront();
		for (size_t i = 0; i < span->n_; i++)
		{
			//idSpanMap_[span->id_ + i] = span;
			idSpanMap_.set(span->id_ + i, span);
		} 
		return span;
	}

	//走到这里表示对应的哈希桶下没有span了，向后遍历查看是否还有空闲的span，
	//如果有的话进行切分
	index++;
	while (index < N_PAGES)
	{
		if (!spanLists_[index].Empty())
		{
			//切分nspan
			Span* nSpan = spanLists_[index].PopFront();
			//Span* kSpan = new Span();
			Span* kSpan = spanPool_.New();
			kSpan->id_ = nSpan->id_;
			kSpan->n_ = k;
		
			//处理id和span的映射,所有id都要映射
			for (size_t i = 0; i < kSpan->n_; i++)
			{
				//idSpanMap_[kSpan->id_ + i] = kSpan;
				idSpanMap_.set(kSpan->id_ + i, kSpan);
			}

			//nspan暂时离开PageManager，不用全部映射，映射头和尾id即可用来合并
			nSpan->id_ += k;
			nSpan->n_ -= k;
			/*idSpanMap_[nSpan->id_] = nSpan;
			idSpanMap_[nSpan->id_ + nSpan->n_ - 1] = nSpan;*/
			idSpanMap_.set(nSpan->id_, nSpan);
			idSpanMap_.set(nSpan->id_ + nSpan->n_ - 1, nSpan);


			//将切好了剩余的span挂入对应的哈希桶中
			spanLists_[nSpan->n_].PushFront(nSpan);
			return kSpan;
		}
		else
		{
			index++;
		}
	}

	//走到这，表示当前PageCache中没有合适的Span了，需要向堆申请内存
	//Span* span = new Span();
	Span* span = spanPool_.New();
	void* memory = SystemAlloc(N_PAGES - 1);
	span->id_ = reinterpret_cast<PAGE_ID>(memory) >> PAGE_SHIFT;
	span->n_ = N_PAGES - 1;
	spanLists_[N_PAGES - 1].PushFront(span);
	//递归调用，提高复用性，因为cpu很快，即使再重复直接前面的一段逻辑也非常块
	//若采用非递归来写，直接将span切分即可，但和上述代码逻辑重复
	return NewSpan(k);
}

// 获取从对象到span的映射
Span* PageManager::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (reinterpret_cast<PAGE_ID>(obj) >> PAGE_SHIFT);
	std::unique_lock<std::mutex> ul(mtx_);
	//auto ret = idSpanMap_.find(id);
	/*if (ret != idSpanMap_.end())
	{
		return ret->second;
	}*/
	void* ret = idSpanMap_.get(id);
	if (ret != nullptr)
	{
		return reinterpret_cast<Span*>(ret);
	}
	else
	{
		//正常情况下不可能找不到
		assert(false);
		return nullptr;
	}
}

// 释放空闲span回到PageManager，并合并相邻的span
void PageManager::ReleaseSpanToPageManager(Span* span)
{
	//大于128页的直接释放
	
	if (span->n_ > N_PAGES - 1)
	{
		void* memory = reinterpret_cast<void*>(span->id_ << PAGE_SHIFT);
		SystemFree(memory);
		//idSpanMap_.erase(span->id_);
		//delete span;
		spanPool_.Delete(span);
		return;
	}
	//检查前面有没有空闲的span，有的话进行合并
	while (1)
	{
		PAGE_ID prevId = span->id_ - 1;
		//auto ret = idSpanMap_.find(prevId);
		/*if (ret == idSpanMap_.end())
		{
			break;
		}*/
		void* ret = idSpanMap_.get(prevId);
		if (ret == nullptr)
		{
			break;
		}
		//Span* prevSpan = ret->second;
		Span* prevSpan = reinterpret_cast<Span*>(ret);
		//前面的页不空闲
		if (prevSpan->isUse_ == true)
		{
			break;
		}

		//如果合并了太长了的话也没法管理
		if (span->n_ + prevSpan->n_ > N_PAGES - 1)
		{
			break;
		}

		//走到这里才可以合并
		span->id_ = prevSpan->id_;
		span->n_ += prevSpan->n_;
		spanLists_[prevSpan->n_].Erase(prevSpan);
		//delete prevSpan;
		spanPool_.Delete(prevSpan);
	}
	//检查后面有没有空闲的span，有的话进行合并
	while (1)
	{
		PAGE_ID nextId = span->id_ + span->n_;
		/*auto ret = idSpanMap_.find(nextId);
		if (ret == idSpanMap_.end())
		{
			break;
		}*/
		void* ret = idSpanMap_.get(nextId);
		if (ret == nullptr)
		{
			break;
		}
		//Span* nextSpan = ret->second;
		Span* nextSpan = reinterpret_cast<Span*>(ret);
		//不空闲
		if (nextSpan->isUse_ == true)
		{
			break;
		}
		//如果合并了太长了的话也没法管理
		if (nextSpan->n_ + span->n_ > N_PAGES - 1)
		{
			break;
		}
		//走到这里可以合并了
		span->n_ += nextSpan->n_;
		spanLists_[nextSpan->n_].Erase(nextSpan);
		//delete nextSpan;
		spanPool_.Delete(nextSpan);
	}

	//走到这里可以直接挂入PageManager了
	spanLists_[span->n_].PushFront(span);
	span->isUse_ = false;
	//将首尾id映射
	/*idSpanMap_[span->id_] = span;
	idSpanMap_[span->id_ + span->n_ - 1] = span;*/
	idSpanMap_.set(span->id_, span);
	idSpanMap_.set(span->id_ + span->n_ - 1, span);
}
 