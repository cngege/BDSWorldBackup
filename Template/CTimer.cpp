#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif


#ifdef _MSC_VER 
# include <sys/timeb.h>
//#include <process.h>
//#include <Windows.h>

#else
#include <sys/time.h>
#include <pthread.h>  
#endif


#include "CTimer.h"

//////////////////////////////////////////////////////////////////////////
// TimerManager


TimerManager::TimerManager(int miniseconds) : m_interval(miniseconds)
{
	m_stop_flag.store(true);
	StartTimerManager();
}

TimerManager::~TimerManager()
{
	StopTimerManager();
}

bool TimerManager::StartTimerManager()
{
	StopTimerManager();
	m_stop_flag.store(false);
	m_thread = std::move(std::thread([this] {this->run(); }));
	return true;
}

void TimerManager::run()
{
	//std::cout << "begin Timer Manager at thread: " << std::this_thread::get_id() << std::endl;
	while (!m_stop_flag)	{
		DetectTimers();
		std::this_thread::sleep_for(std::chrono::milliseconds(m_interval));
	}
	//std::cout <<"end Timer Manager thread ID: " << std::this_thread::get_id() << std::endl;
}

void TimerManager::StopTimerManager()
{
	m_stop_flag.store(true);
	if(m_thread.joinable())
		m_thread.join();
}


void TimerManager::AddTimer(Timer* timer)
{
	timer->m_heapIndex = heap_.size();
	HeapEntry entry = { timer->m_expires, timer };
	heap_.push_back(entry);
	UpHeap(heap_.size() - 1);
}

void TimerManager::RemoveTimer(Timer* timer)
{
	size_t index = timer->m_heapIndex;
	if (!heap_.empty() && index < heap_.size())
	{
		if (index == heap_.size() - 1)
		{
			heap_.pop_back();
		}
		else
		{
			SwapHeap(index, heap_.size() - 1);
			heap_.pop_back();
			size_t parent = (index - 1) / 2;
			if (index > 0 && heap_[index].time < heap_[parent].time)
				UpHeap(index);
			else
				DownHeap(index);
		}
	}
}

void TimerManager::DetectTimers()
{
	unsigned long long now = GetCurrentMillisecs();

	while (!heap_.empty() && heap_[0].time <= now)
	{
		Timer* timer = heap_[0].timer;
		RemoveTimer(timer);
		timer->OnTimer(now);
	}
}

void TimerManager::UpHeap(size_t index)
{
	size_t parent = (index - 1) / 2;
	while (index > 0 && heap_[index].time < heap_[parent].time)
	{
		SwapHeap(index, parent);
		index = parent;
		parent = (index - 1) / 2;
	}
}

void TimerManager::DownHeap(size_t index)
{
	size_t child = index * 2 + 1;
	while (child < heap_.size())
	{
		size_t minChild = (child + 1 == heap_.size() || heap_[child].time < heap_[child + 1].time)
			? child : child + 1;
		if (heap_[index].time < heap_[minChild].time)
			break;
		SwapHeap(index, minChild);
		index = minChild;
		child = index * 2 + 1;
	}
}

void TimerManager::SwapHeap(size_t index1, size_t index2)
{
	HeapEntry tmp = heap_[index1];
	heap_[index1] = heap_[index2];
	heap_[index2] = tmp;
	heap_[index1].timer->m_heapIndex = index1;
	heap_[index2].timer->m_heapIndex = index2;
}

unsigned long long TimerManager::GetCurrentMillisecs()
{
#ifdef _MSC_VER
	_timeb timebuffer;
	_ftime64_s(&timebuffer);
	unsigned long long ret = timebuffer.time;
	ret = ret * 1000 + timebuffer.millitm;
	return ret;
#else
	timeval tv;
	::gettimeofday(&tv, 0);
	unsigned long long ret = tv.tv_sec;
	ret = ret * 1000 + tv.tv_usec / 1000;
#endif
}

