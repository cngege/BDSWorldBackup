/*  用法与用量

static void TimerHandler1(void* p)
{
	printf("TimerHandler11111  x = %d\n",*(int*)p );
}
static void TimerHander2(void* p)
{
	printf("Timer22\n");
}

	TimerManager tm( 100 );
	tm.StartTimerManager();
	Timer t(tm);
	Timer t2(tm);

	int x = 0;
	t.Start(TimerHandler1, 1000, &x);
	t2.Start(TimerHander2,1333);

	//停止
	tm.StopTimerManager();
*/ 

#ifndef _PETER_C_TIMER_
#define _PETER_C_TIMER_


#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>


//定时器回调函数定义
//typedef   void(*TimerFunction)(void* p);
using _timerFun = std::function<void()>;

class Timer;

// 单线程 定时器管理器
class TimerManager
{
public:
	TimerManager(int miniseconds = 200);
	~TimerManager();
	static unsigned long long GetCurrentMillisecs();

	void StopTimerManager();
	bool StartTimerManager();

	void run();

private:

	friend class Timer;
	void AddTimer(Timer* timer);
	void RemoveTimer(Timer* timer);


	void UpHeap(size_t index);
	void DownHeap(size_t index);
	void SwapHeap(size_t, size_t index2);
	void DetectTimers();

private:
	int                   m_interval;
	std::atomic<bool>     m_stop_flag;
	std::thread           m_thread;
	struct HeapEntry
	{
		unsigned long long time;
		Timer* timer;
	};
	std::vector<HeapEntry> heap_;
};



class Timer
{
public:
	enum TimerType { ONCE, CIRCLE };

	Timer(TimerManager& manager) : m_manager(manager),m_heapIndex(-1) {};
	~Timer() { Stop(); };

	template<class Fun>
	void Start(Fun fun, unsigned interval, TimerType timeType = CIRCLE) {
		Stop();
		m_interval = interval;
		m_fun = fun;
		m_timerType = timeType;
		this->m_expires = this->m_interval + TimerManager::GetCurrentMillisecs();
		m_manager.AddTimer(this);
	};
	void Stop() {
		if (m_heapIndex != -1){
			m_manager.RemoveTimer(this);
			m_heapIndex = -1;
		}
	};

private:
	void OnTimer(unsigned long long now) {
		if (m_timerType == Timer::CIRCLE)
		{
			m_expires = m_interval + now;
			m_manager.AddTimer(this);
		}
		else
		{
			m_heapIndex = -1;
		}
		m_fun();
	};

private:
	friend class   TimerManager;
	TimerManager&  m_manager;
	TimerType      m_timerType;
	_timerFun      m_fun;
	unsigned int   m_interval;
	unsigned long long m_expires;
	size_t		   m_heapIndex;
};



#endif
