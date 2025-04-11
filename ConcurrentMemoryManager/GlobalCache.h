#pragma once
#include "Common.h"

//中心缓存  单例模式c++11懒汉模式
class GlobalCache
{
public:
	//获取单例对象
	static GlobalCache* GetInstance();
	
	// 获取⼀个⾮空的span
	Span* GetOneSpan(SpanList& list, size_t size);
	// 从中⼼缓存获取⼀定数量的对象给local cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// 将⼀定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t size);

private:
	//防止构造和拷贝
	GlobalCache(){}
	GlobalCache(const GlobalCache&) = delete;
private:
	SpanList spanLists_[N_LISTS];
	
};