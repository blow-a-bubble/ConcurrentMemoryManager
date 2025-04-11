#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"
//ҳ����   ����ģʽc++98 ����ģʽ
class PageManager
{
public:
	static PageManager* GetInstance();
	//��ȡ����kҳ�ڴ��span
	Span* NewSpan(size_t k);
	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);
	// �ͷſ���span�ص�PageManager�����ϲ����ڵ�span
	void ReleaseSpanToPageManager(Span* span);
private:
	PageManager(){}
	PageManager(const PageManager&) = delete;
private:
	SpanList spanLists_[N_PAGES]; //ÿ����ϣͰ����һЩû�б��зֵ�span
	static PageManager* singleInstance_;
	//����������
	static std::mutex classMtx_;
	//std::unordered_map<PAGE_ID, Span*> idSpanMap_;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> idSpanMap_;
	ObjectPool<Span> spanPool_;

public:
	std::mutex mtx_; //ֱ�ӽ�public����Ϊ�����޸�
};
