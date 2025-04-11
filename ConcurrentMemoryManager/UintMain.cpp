#define  _CRT_SECURE_NO_WARNINGS
#include "ObjectPool.h"
#include "ConcurrentAllocate.h"

void Alloc1()
{
	for (size_t i = 0; i < 5; i++)
	{
		void* obj = ConcurrentAllocate(6);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; i++)
	{
		void* obj = ConcurrentAllocate(4);
	}
}
//thread local storage
void TestTLS()
{
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);
	t1.join();
	t2.join();
}

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAllocate(1);
	void* p2 = ConcurrentAllocate(2);
	void* p3 = ConcurrentAllocate(8);
	void* p4 = ConcurrentAllocate(6);
	void* p5 = ConcurrentAllocate(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
}

void TestConcurrentAlloc2()
{
	for (size_t i = 0; i < 1024; i++)
	{
		void* p1 = ConcurrentAllocate(6);
		cout << p1 << endl;
	}
	void* p2 = ConcurrentAllocate(8);
	cout << p2 << endl;
}

void TestConcurrentDeAllocate()
{
	void* p1 = ConcurrentAllocate(1);
	void* p2 = ConcurrentAllocate(2);
	void* p3 = ConcurrentAllocate(8);
	void* p4 = ConcurrentAllocate(6);
	void* p5 = ConcurrentAllocate(8);
	void* p6 = ConcurrentAllocate(2);
	void* p7 = ConcurrentAllocate(8);


	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;
	cout << p6 << endl;
	cout << p7 << endl;



	ConcurrentDeAllocate(p1);
	ConcurrentDeAllocate(p2);
	ConcurrentDeAllocate(p3);
	ConcurrentDeAllocate(p4);
	ConcurrentDeAllocate(p5);
	ConcurrentDeAllocate(p6);
	ConcurrentDeAllocate(p7);

}

void ThreadAlloc1()
{
	std::vector<void*> allocs;
	for (size_t i = 0; i < 7; i++)
	{
		void* obj = ConcurrentAllocate(6);
		allocs.push_back(obj);
	}

	for (auto& e : allocs)
	{
		ConcurrentDeAllocate(e);
	}
}

void ThreadAlloc2()
{
	std::vector<void*> allocs;
	for (size_t i = 0; i < 7; i++)
	{
		void* obj = ConcurrentAllocate(6);
		allocs.push_back(obj);
	}

	for (auto& e : allocs)
	{
		ConcurrentDeAllocate(e);
	}
}

void TestThreadAlloc()
{
	std::thread t1(ThreadAlloc1);
	std::thread t2(ThreadAlloc2);
	t1.join();
	t2.join();
}

void TestBigAlloc()
{
	void* p1 = ConcurrentAllocate(257 * 1024);
	ConcurrentDeAllocate(p1);

	void* p2 = ConcurrentAllocate(N_PAGES << PAGE_SHIFT);
	ConcurrentDeAllocate(p2);
}
//int main()
//{
//	//TestObjectPool();
//	//TestTLS();
//	TestConcurrentAlloc2(); 
//	//TestConcurrentDeAllocate();
//	//TestThreadAlloc();
//	TestBigAlloc();
//	return 0;
//}