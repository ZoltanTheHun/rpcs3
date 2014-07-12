#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <Utilities/SSemaphore.h>

static std::thread::id main_thread;

struct rThread
{
	static bool IsMain() { std::this_thread::get_id() == main_thread; }
};

class ThreadExec;

class NamedThreadBase
{
	std::string m_name;

	std::condition_variable m_signal_cv;
	std::mutex m_signal_mtx;

public:
	NamedThreadBase(const std::string& name) : m_name(name)
	{
	}

	NamedThreadBase()
	{
	}

	virtual std::string GetThreadName() const;
	virtual void SetThreadName(const std::string& name);

	void WaitForAnySignal() // wait 1 ms for something
	{
		std::unique_lock<std::mutex> lock(m_signal_mtx);
		m_signal_cv.wait_for(lock, std::chrono::milliseconds(1));
	}

	void Notify() // wake up waiting thread or nothing
	{
		m_signal_cv.notify_one();
	}
};

NamedThreadBase* GetCurrentNamedThread();

class ThreadBase : public NamedThreadBase
{
protected:
	std::atomic<bool> m_destroy;
	std::atomic<bool> m_alive;
	std::thread* m_executor;

	mutable std::mutex m_main_mutex;

	ThreadBase(const std::string& name);
	~ThreadBase();

public:
	void Start();
	void Stop(bool wait = true, bool send_destroy = true);

	bool Join() const;
	bool IsAlive() const;
	bool TestDestroy() const;

	virtual void Task() = 0;
};

class thread
{
	std::string m_name;
	std::thread m_thr;

public:
	thread(const std::string& name, std::function<void()> func);
	thread(const std::string& name);
	thread();


public:
	void start(std::function<void()> func);
	void detach();
	void join();
	bool joinable() const;
};

template<typename T> class MTPacketBuffer
{
protected:
	volatile bool m_busy;
	volatile u32 m_put, m_get;
	std::vector<u8> m_buffer;
	u32 m_max_buffer_size;
	mutable std::recursive_mutex m_cs_main;

	void CheckBusy()
	{
		m_busy = m_put >= m_max_buffer_size;
	}

public:
	MTPacketBuffer(u32 max_buffer_size)
		: m_max_buffer_size(max_buffer_size)
	{
		Flush();
	}

	~MTPacketBuffer()
	{
		Flush();
	}

	void Flush()
	{
		std::lock_guard<std::recursive_mutex> lock(m_cs_main);
		m_put = m_get = 0;
		m_buffer.clear();
		m_busy = false;
	}

private:
	virtual void _push(const T& v) = 0;
	virtual T _pop() = 0;

public:
	void Push(const T& v)
	{
		std::lock_guard<std::recursive_mutex> lock(m_cs_main);
		_push(v);
	}

	T Pop()
	{
		std::lock_guard<std::recursive_mutex> lock(m_cs_main);
		return _pop();
	}

	bool HasNewPacket() const { std::lock_guard<std::recursive_mutex> lock(m_cs_main); return m_put != m_get; }
	bool IsBusy() const { return m_busy; }
};

/*
class StepThread : public ThreadBase
{
	wxSemaphore m_main_sem;
	wxSemaphore m_destroy_sem;
	volatile bool m_exit;

protected:
	StepThread(const std::string& name = "Unknown StepThread")
		: ThreadBase(true, name)
		, m_exit(false)
	{
	}

	virtual ~StepThread() throw()
	{
	}

private:
	virtual void Task()
	{
		m_exit = false;

		while(!TestDestroy())
		{
			m_main_sem.Wait();

			if(TestDestroy() || m_exit) break;

			Step();
		}

		while(!TestDestroy()) std::this_thread::sleep_for(std::chrono::milliseconds(0));
		if(m_destroy_sem.TryWait() != wxSEMA_NO_ERROR) m_destroy_sem.Post();
	}

	virtual void Step()=0;

public:
	void DoStep()
	{
		if(IsRunning()) m_main_sem.Post();
	}

	void WaitForExit()
	{
		if(TestDestroy()) m_destroy_sem.Wait();
	}

	void WaitForNextStep()
	{
		if(!IsRunning()) return;

		while(m_main_sem.TryWait() != wxSEMA_NO_ERROR) std::this_thread::sleep_for(std::chrono::milliseconds(0));
	}

	void Exit(bool wait = false)
	{
		if(!IsAlive()) return;

		if(m_main_sem.TryWait() != wxSEMA_NO_ERROR)
		{
			m_exit = true;
			m_main_sem.Post();
		}

		Delete();

		if(wait) WaitForExit();
	}
};
*/
