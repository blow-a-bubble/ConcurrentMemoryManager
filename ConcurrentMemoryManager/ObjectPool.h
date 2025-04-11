#pragma once
#include "Common.h"


//定长内存池
//template<size_t N>
//class ObjectPool
//{};


//定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//先去自由链表申请
		//自由链表中有内存
		if (freeList_)
		{
			//头删链表
			void* next = *reinterpret_cast<void**>(freeList_);
			obj = reinterpret_cast<T*>(freeList_);
			freeList_ = next;
		}
		//自由链表中没有内存
		else
		{
			//大内存块内存不足
			if (lastSize_ < sizeof(T))
			{
				lastSize_ = 128 * 1024;
				//memory_ = reinterpret_cast<char*>(malloc(lastSize_));

				memory_ = reinterpret_cast<char*>(SystemAlloc(lastSize_ >> PAGE_SHIFT));
				if (memory_ == nullptr)
				{
					throw std::bad_alloc();
				}
			}

			obj = reinterpret_cast<T*>(memory_);
			//若申请的内存大小连一个地址(4或8字节)都没有，直接讲申请的内存大小变成一个地址大小
			//为了方便以后还内存时候链表的处理
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			memory_ += objSize;
			lastSize_ -= objSize;
		}

		//定位new初始化
		new(obj)T();
		return obj;
	}

	void Delete(T* obj)
	{
		//手动调用析构函数
		obj->~T();

		//头插入自由链表
		*reinterpret_cast<void**>(obj) = freeList_;
		freeList_ = obj;
	}
private:
	char* memory_ = nullptr;     //大块内存地址, 定义成char* -> 方便移动指针
	size_t lastSize_ = 0;         //大内存快中剩余的内存大小  单位:字节
	void* freeList_ = nullptr;   //自由链表头指针   自由链表: 标识还回来的内存   
};

//struct TreeNode
//{
//	TreeNode* left_;
//	TreeNode* right_;
//	int val_;
//
//	TreeNode()
//		:left_(nullptr)
//		, right_(nullptr)
//		, val_(0)
//	{}
//};
////测试定长内存池
//void TestObjectPool()
//{
//	ObjectPool<TreeNode> pool;
//	size_t times = 3;     //实验次数
//	size_t count = 100000; //每次申请和释放对象个数
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(count);
//
//	size_t start1 = clock();
//	for (size_t i = 0; i < times; ++i)
//	{
//		for (size_t j = 0; j < count; j++)
//		{
//			v1.push_back(new TreeNode());
//		}
//		for (size_t j = 0; j < count; j++)
//		{
//			delete v1[j];
//		}
//		v1.clear();
//	}
//	size_t end1 = clock();
//
//
//	std::vector<TreeNode*> v2;
//	v1.reserve(count);
//	size_t start2 = clock();
//	for (size_t i = 0; i < times; ++i)
//	{
//		for (size_t j = 0; j < count; j++)
//		{
//			v2.push_back(pool.New());
//		}
//		for (size_t j = 0; j < count; j++)
//		{
//			pool.Delete(v2[i]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	cout << "new cost time:" << end1 - start1 << endl;
//	cout << "object pool cost time:" << end2 - start2 << endl;
//}