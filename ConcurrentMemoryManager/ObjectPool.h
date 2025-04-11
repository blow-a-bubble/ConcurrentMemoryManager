#pragma once
#include "Common.h"


//�����ڴ��
//template<size_t N>
//class ObjectPool
//{};


//�����ڴ��
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//��ȥ������������
		//�������������ڴ�
		if (freeList_)
		{
			//ͷɾ����
			void* next = *reinterpret_cast<void**>(freeList_);
			obj = reinterpret_cast<T*>(freeList_);
			freeList_ = next;
		}
		//����������û���ڴ�
		else
		{
			//���ڴ���ڴ治��
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
			//��������ڴ��С��һ����ַ(4��8�ֽ�)��û�У�ֱ�ӽ�������ڴ��С���һ����ַ��С
			//Ϊ�˷����Ժ��ڴ�ʱ������Ĵ���
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			memory_ += objSize;
			lastSize_ -= objSize;
		}

		//��λnew��ʼ��
		new(obj)T();
		return obj;
	}

	void Delete(T* obj)
	{
		//�ֶ�������������
		obj->~T();

		//ͷ������������
		*reinterpret_cast<void**>(obj) = freeList_;
		freeList_ = obj;
	}
private:
	char* memory_ = nullptr;     //����ڴ��ַ, �����char* -> �����ƶ�ָ��
	size_t lastSize_ = 0;         //���ڴ����ʣ����ڴ��С  ��λ:�ֽ�
	void* freeList_ = nullptr;   //��������ͷָ��   ��������: ��ʶ���������ڴ�   
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
////���Զ����ڴ��
//void TestObjectPool()
//{
//	ObjectPool<TreeNode> pool;
//	size_t times = 3;     //ʵ�����
//	size_t count = 100000; //ÿ��������ͷŶ������
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