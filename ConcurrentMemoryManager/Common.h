#pragma once
#include <iostream>
#include <vector>
#include <ctime>
#include <cassert>
#include <thread>
#include <mutex>
#include <algorithm>
#include <unordered_map>
#ifdef _WIN32
//windows
#include <Windows.h>
#else
//linux
#endif
using std::cout;
using std::endl;

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#endif

static const size_t MAX_BYTES = 256 * 1024;
const static size_t N_LISTS = 208;
static const size_t N_PAGES = 129;
static const size_t PAGE_SHIFT = 13;   // 2^13 即8k

// 直接去堆上按?申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}


//找到下一个节点
static inline void*& NextObj(void* obj)
{
	assert(obj);
	return *reinterpret_cast<void**>(obj);
}
//自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		//头插
		NextObj(obj) = freeList_;
		freeList_ = obj;
		size_++;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = freeList_;
		freeList_ = start;
		size_ += n;
	}
	void* Pop()
	{
		assert(freeList_);
		//头删
		void* obj = freeList_;
		freeList_ = NextObj(freeList_);
		size_--;
		return obj;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= Size());
		start = freeList_;
		end = start;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = NextObj(end);
		}
		freeList_ = NextObj(end);
		NextObj(end) = nullptr;
		size_ -= n;
	}
	bool Empty()
	{
		return freeList_ == nullptr;
	}

	size_t& MaxSize()
	{
		return maxSize_;
	}
	size_t& Size()
	{
		return size_;
	}
private:
	void* freeList_ = nullptr;
	
	size_t maxSize_ = 1; //支持慢启动调节机制的计数器

	size_t size_ = 0;//内存块个数
};


//管理大小和哈希桶的映射关系
class SizeClass
{
	// 整体控制在最多10%左右的内碎⽚浪费
	// [1,128]					8byte对⻬       freelist[0, 16)
	// [128+1,1024]				16byte对⻬		freelist[16, 72)
	// [1024+1,8*1024]			128byte对⻬		freelist[72, 128)
	// [8*1024+1,64*1024]		1024byte对齐	freelist[128, 184)
	// [64*1024+1,256*1024]		8*1024byte对齐	freelist[184, 208)        
public:
	/*static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		if (size % alignNum == 0)
		{
			return size;
		}
		else
		{
			return (size / alignNum + 1) * alignNum;
		}
	}*/

	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return (((bytes)+alignNum - 1) & ~(alignNum - 1));
	}


	//根据对齐数向上取整
	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if(size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}


	/*static inline size_t _Index(size_t size, size_t alignNum)
	{
		if (size % alignNum == 0)
		{
			return size / alignNum - 1;
		}
		else
		{
			return size / alignNum;
		}
	}*/

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	//找到对应的哈希桶下表
	static inline size_t Index(size_t size)
	{
		int arr[] = { 16, 56, 56, 56, 26 }; //每个对齐数所站的哈希桶个数
		if (size <= 128)
		{
			//return _Index(size, 8);
			return _Index(size, 3);

		}
		else if (size <= 1024)
		{
			//return _Index(size - 128, 16) + arr[0];
			return _Index(size, 4) + arr[0];

		}
		else if (size <= 8 * 1024)
		{
			//return _Index(size - 1024, 128) + arr[0] + arr[1];
			return _Index(size - 1024, 7) + arr[0] + arr[1];

		}
		else if (size < 64 * 1024)
		{
			//return _Index(size - 8 * 1024, 1024) + arr[0] + arr[1] + arr[2];
			return _Index(size - 8 * 1024, 10) + arr[0] + arr[1] + arr[2];

		}
		else if (size < 256 * 1024)
		{
			//return _Index(size - 64 * 1024, 8 * 1024) + arr[0] + arr[1] + arr[2] + arr[3];
			return _Index(size - 64 * 1024, 13) + arr[0] + arr[1] + arr[2] + arr[3];

		}
		else
		{
			assert(false);
			return -1;
		}
	}


	// 根据获取一个内存大小计算⼀次从中⼼缓存获取多少个
	static inline size_t NumMoveSize(size_t size)
	{
		size_t num = MAX_BYTES / size; //申请的内存越大，获取的数量就相对少了

		//控制数量的边界
		if (num < 2)
		{
			num = 2;
		}
		if (num > 512)
		{
			num = 512;
		}
		return num;
	}

	// 计算⼀次向系统获取⼏个⻚

		// 单个对象  8byte
		// ...
		// 单个对象  256KB
	static inline size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}
};


//管理批量页的大内存块
struct Span
{
	PAGE_ID id_ = 0; //起始页的id
	size_t n_ = 0;   //页的数量

	Span* prev_ = nullptr; //双向链表
	Span* next_ = nullptr;

	void* freeList_ = nullptr; //用自由链表管理切好的小内存块

	size_t use_count_ = 0; //被使用的小内存块数量，为0表示都没被使用，可以被PageCache回收

	bool isUse_ = false; //是否被使用

	size_t objectSize_ = 0; //小内存块的大小
};


//带头双向链表
class SpanList
{
public:
	SpanList()
	{
		head_ = new Span();
		head_->next_ = head_;
		head_->prev_ = head_;
	}

	Span* Begin()
	{
		return head_->next_;
	}

	Span* End()
	{
		return head_;
	}
	bool Empty()
	{
		return head_->next_ == head_;
	}

	

	void PushFront(Span* obj)
	{
		Insert(Begin(), obj);
	}
	void Insert(Span* pos, Span* obj)
	{
		Span* prev = pos->prev_;
		prev->next_ = obj;
		obj->prev_ = prev;
		
		obj->next_ = pos;
		pos->prev_ = obj;
	}

	Span* PopFront()
	{
		Span* span = Begin();
		Erase(span);
		return span;
	}
	void Erase(Span* pos)
	{
		assert(pos != head_);
		Span* prev = pos->prev_;
		Span* next = pos->next_;
		prev->next_ = next;
		next->prev_ = prev;
	}

private:
	Span* head_ = nullptr;    //头节点
public:
	std::mutex mtx_;         //桶锁,直接public因为锁无法修改
};


