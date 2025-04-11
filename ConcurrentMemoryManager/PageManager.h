#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"
//页缓存   单例模式c++98 懒汉模式
class PageManager
{
public:
	static PageManager* GetInstance();
	//获取管理k页内存的span
	Span* NewSpan(size_t k);
	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);
	// 释放空闲span回到PageManager，并合并相邻的span
	void ReleaseSpanToPageManager(Span* span);
private:
	PageManager(){}
	PageManager(const PageManager&) = delete;
private:
	SpanList spanLists_[N_PAGES]; //每个哈希桶挂着一些没有被切分的span
	static PageManager* singleInstance_;
	//管理单例的锁
	static std::mutex classMtx_;
	//std::unordered_map<PAGE_ID, Span*> idSpanMap_;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> idSpanMap_;
	ObjectPool<Span> spanPool_;

public:
	std::mutex mtx_; //直接将public，因为不可修改
};
