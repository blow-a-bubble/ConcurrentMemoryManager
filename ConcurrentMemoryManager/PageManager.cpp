#define  _CRT_SECURE_NO_WARNINGS
#include "PageManager.h"
PageManager* PageManager::singleInstance_ = nullptr;
std::mutex PageManager::classMtx_;
PageManager* PageManager::GetInstance()
{
	//˫���
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

//��ȡ����kҳ�ڴ��span
Span* PageManager::NewSpan(size_t k)
{
	assert(k > 0);
	//����128ҳ��ֱ���������
	if (k > N_PAGES - 1)
	{
		//Span* span = new Span();
		Span* span = spanPool_.New();
		void* memory = SystemAlloc(k);
		span->id_ = (reinterpret_cast<PAGE_ID>(memory)) >> PAGE_SHIFT;
		span->n_ = k;
		//ӳ���һҳ���ɣ���Ϊ����ڴ治�ڹ�ϣͰ�У����úϲ����֣������ͷŵ�ʱ��϶����õ�һҳ�ĵ�ַ���ͷ�
		//idSpanMap_[span->id_] = span;
		idSpanMap_.set(span->id_, span);
		return span;
	}
	//PageCache��k���±�һһ��Ӧ��0�±�ճ���������������ӳ���ϵ
	size_t index = k;
	//�����Ӧ��ϣͰ����span
	if (!spanLists_[index].Empty())
	{
		//����id��span��ӳ��,����id��Ҫӳ��
		Span* span = spanLists_[index].PopFront();
		for (size_t i = 0; i < span->n_; i++)
		{
			//idSpanMap_[span->id_ + i] = span;
			idSpanMap_.set(span->id_ + i, span);
		} 
		return span;
	}

	//�ߵ������ʾ��Ӧ�Ĺ�ϣͰ��û��span�ˣ��������鿴�Ƿ��п��е�span��
	//����еĻ������з�
	index++;
	while (index < N_PAGES)
	{
		if (!spanLists_[index].Empty())
		{
			//�з�nspan
			Span* nSpan = spanLists_[index].PopFront();
			//Span* kSpan = new Span();
			Span* kSpan = spanPool_.New();
			kSpan->id_ = nSpan->id_;
			kSpan->n_ = k;
		
			//����id��span��ӳ��,����id��Ҫӳ��
			for (size_t i = 0; i < kSpan->n_; i++)
			{
				//idSpanMap_[kSpan->id_ + i] = kSpan;
				idSpanMap_.set(kSpan->id_ + i, kSpan);
			}

			//nspan��ʱ�뿪PageManager������ȫ��ӳ�䣬ӳ��ͷ��βid���������ϲ�
			nSpan->id_ += k;
			nSpan->n_ -= k;
			/*idSpanMap_[nSpan->id_] = nSpan;
			idSpanMap_[nSpan->id_ + nSpan->n_ - 1] = nSpan;*/
			idSpanMap_.set(nSpan->id_, nSpan);
			idSpanMap_.set(nSpan->id_ + nSpan->n_ - 1, nSpan);


			//���к���ʣ���span�����Ӧ�Ĺ�ϣͰ��
			spanLists_[nSpan->n_].PushFront(nSpan);
			return kSpan;
		}
		else
		{
			index++;
		}
	}

	//�ߵ��⣬��ʾ��ǰPageCache��û�к��ʵ�Span�ˣ���Ҫ��������ڴ�
	//Span* span = new Span();
	Span* span = spanPool_.New();
	void* memory = SystemAlloc(N_PAGES - 1);
	span->id_ = reinterpret_cast<PAGE_ID>(memory) >> PAGE_SHIFT;
	span->n_ = N_PAGES - 1;
	spanLists_[N_PAGES - 1].PushFront(span);
	//�ݹ���ã���߸����ԣ���Ϊcpu�ܿ죬��ʹ���ظ�ֱ��ǰ���һ���߼�Ҳ�ǳ���
	//�����÷ǵݹ���д��ֱ�ӽ�span�зּ��ɣ��������������߼��ظ�
	return NewSpan(k);
}

// ��ȡ�Ӷ���span��ӳ��
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
		//��������²������Ҳ���
		assert(false);
		return nullptr;
	}
}

// �ͷſ���span�ص�PageManager�����ϲ����ڵ�span
void PageManager::ReleaseSpanToPageManager(Span* span)
{
	//����128ҳ��ֱ���ͷ�
	
	if (span->n_ > N_PAGES - 1)
	{
		void* memory = reinterpret_cast<void*>(span->id_ << PAGE_SHIFT);
		SystemFree(memory);
		//idSpanMap_.erase(span->id_);
		//delete span;
		spanPool_.Delete(span);
		return;
	}
	//���ǰ����û�п��е�span���еĻ����кϲ�
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
		//ǰ���ҳ������
		if (prevSpan->isUse_ == true)
		{
			break;
		}

		//����ϲ���̫���˵Ļ�Ҳû������
		if (span->n_ + prevSpan->n_ > N_PAGES - 1)
		{
			break;
		}

		//�ߵ�����ſ��Ժϲ�
		span->id_ = prevSpan->id_;
		span->n_ += prevSpan->n_;
		spanLists_[prevSpan->n_].Erase(prevSpan);
		//delete prevSpan;
		spanPool_.Delete(prevSpan);
	}
	//��������û�п��е�span���еĻ����кϲ�
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
		//������
		if (nextSpan->isUse_ == true)
		{
			break;
		}
		//����ϲ���̫���˵Ļ�Ҳû������
		if (nextSpan->n_ + span->n_ > N_PAGES - 1)
		{
			break;
		}
		//�ߵ�������Ժϲ���
		span->n_ += nextSpan->n_;
		spanLists_[nextSpan->n_].Erase(nextSpan);
		//delete nextSpan;
		spanPool_.Delete(nextSpan);
	}

	//�ߵ��������ֱ�ӹ���PageManager��
	spanLists_[span->n_].PushFront(span);
	span->isUse_ = false;
	//����βidӳ��
	/*idSpanMap_[span->id_] = span;
	idSpanMap_[span->id_ + span->n_ - 1] = span;*/
	idSpanMap_.set(span->id_, span);
	idSpanMap_.set(span->id_ + span->n_ - 1, span);
}
 