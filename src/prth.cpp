#include "prth.hpp"

#if ENABLE_IOCP

#define NOMINMAX
#include <Windows.h>

#endif

namespace pr
{
#if ENABLE_IOCP
    struct TaskObject
    {
        ThreadPool::Task task;
    };

    ThreadPool::ThreadPool(int nThreads, int64_t nReminedElement)
        :_nThreads(nThreads), _nReminedElement(nReminedElement)
    {
        _iocp = (void *)CreateIoCompletionPort( INVALID_HANDLE_VALUE, 0, 0, nThreads );

        _hasElement = 0 < _nReminedElement;
        _executingCount = 0;

        launchThreads(1);
        if (1 < nThreads)
        {
            _initThreads = std::async(std::launch::async, [nThreads, this] {
                launchThreads(nThreads - 1);
            });
        }
    }
    ThreadPool::~ThreadPool()
    {
        if (_initThreads.valid())
        {
            _initThreads.get();
        }

        CloseHandle((HANDLE)_iocp);

        for (int i = 0; i < _workers.size(); ++i)
        {
            _workers[i].join();
        }
        _workers.clear();
    }

    void ThreadPool::enqueueTask(Task task)
    {
        TaskObject* o = new TaskObject();
        o->task = std::move(task);
        PostQueuedCompletionStatus((HANDLE)_iocp, 0, (ULONG_PTR)o, 0);
    }
    void ThreadPool::enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t, ThreadPool*)> work)
    {
        int nSplit = _nThreads * splitLevel; // this is really adhoc
        int cur = 0;
        for (int i = 0; i < nSplit; ++i)
        {
            int n = (nExecute + i) / nSplit;
            int beg = cur;
            cur += n;
            int end = cur;

            if (beg == end)
            {
                continue;
            }

            enqueueTask([beg, end, work](ThreadPool* p) {
                work(beg, end, p);
            });
        }
    }
    void ThreadPool::addElements(int64_t nElements)
    {
        assert(0 < nElements);

        int64_t prev = _nReminedElement.fetch_add(nElements);
        if (prev == 0)
        {
            std::unique_lock<std::mutex> lockGuard(_statusMutex);
            _hasElement = true;
        }
    }
    void ThreadPool::doneElements(int64_t nElements)
    {
        assert(0 < nElements);

        int64_t previous = _nReminedElement.fetch_sub(nElements);
        if (previous == nElements)
        {
            {
                std::unique_lock<std::mutex> lockGuard(_statusMutex);
                _hasElement = false;
            }
            _statusCondition.notify_all();
        }
    }
    void ThreadPool::waitForDoneElements()
    {
        std::unique_lock<std::mutex> lockGuard(_statusMutex);
        _statusCondition.wait(lockGuard, [this] {
            return _hasElement == false;
        });
    }

    void ThreadPool::launchThreads(int nThreads)
    {
        for (int i = 0; i < nThreads; ++i)
        {
            _workers.emplace_back([this] {
                for (;;)
                {
                    LPOVERLAPPED aux;
                    DWORD        cmd;
                    TaskObject* o = 0;
                    if( GetQueuedCompletionStatus((HANDLE)_iocp, &cmd, (ULONG_PTR*)&o, &aux, INFINITE) == FALSE )
                    {
                        break;
                    }
                    _executingCount++;
                    o->task(this);
                    _executingCount--;
                }
            });
        }
    }
    ThreadPool::ProcessResult ThreadPool::processTask()
    {
        LPOVERLAPPED aux;
        DWORD        cmd;
        TaskObject* o = 0;
        if( GetQueuedCompletionStatus((HANDLE)_iocp, &cmd, (ULONG_PTR*)&o, &aux, 0) == FALSE )
        {
            return ProcessResult::NoTask;
        }
        o->task(this);
        delete o;

        return ProcessResult::Consumed;
    }
#else
	ThreadPool::ThreadPool(int nThreads, int64_t nReminedElement)
        :_nThreads(nThreads), _nReminedElement(nReminedElement)
    {
        _continue = true;
        _hasElement = 0 < _nReminedElement;
        _executingCount = 0;

        launchThreads(1);
        if (1 < nThreads)
        {
            _initThreads = std::async(std::launch::async, [nThreads, this] {
                launchThreads(nThreads - 1);
            });
        }
    }
    ThreadPool::~ThreadPool()
    {
        if (_initThreads.valid())
        {
            _initThreads.get();
        }

        {
            std::unique_lock<std::mutex> lockGuard(_taskMutex);
            _continue = false;
        }
        _taskCondition.notify_all();

        for (int i = 0; i < _workers.size(); ++i)
        {
            _workers[i].join();
        }
        _workers.clear();
    }

    void ThreadPool::enqueueTask(Task task)
    {
        {
            std::unique_lock<std::mutex> lockGuard(_taskMutex);
            _tasks.emplace(task);
        }
        _taskCondition.notify_one();
    }
    void ThreadPool::enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t, ThreadPool *)> work)
    {
        {
            std::unique_lock<std::mutex> lockGuard(_taskMutex);
            int nSplit = _nThreads * splitLevel; // this is really adhoc
            int cur = 0;
            for (int i = 0; i < nSplit; ++i)
            {
                int n = (nExecute + i) / nSplit;
                int beg = cur;
                cur += n;
                int end = cur;
                
                if( beg == end )
                {
                    continue;
                }

                _tasks.emplace([beg, end, work](ThreadPool* p) {
                    work(beg, end, p);
                });
            }
        }
        _taskCondition.notify_all();
    }
    void ThreadPool::addElements(int64_t nElements)
    {
        assert(0 < nElements);

        int64_t prev = _nReminedElement.fetch_add(nElements);
        if (prev == 0)
        {
            std::unique_lock<std::mutex> lockGuard(_statusMutex);
            _hasElement = true;
        }
    }
    void ThreadPool::doneElements(int64_t nElements)
    {
        assert(0 < nElements);

        int64_t previous = _nReminedElement.fetch_sub(nElements);
        if (previous == nElements)
        {
            {
                std::unique_lock<std::mutex> lockGuard(_statusMutex);
                _hasElement = false;
            }
            _statusCondition.notify_all();
        }
    }
    void ThreadPool::waitForDoneElements()
    {
        std::unique_lock<std::mutex> lockGuard(_statusMutex);
        _statusCondition.wait(lockGuard, [this] {
            return _hasElement == false;
        });
    }

    void ThreadPool::launchThreads(int nThreads)
    {
        for (int i = 0; i < nThreads; ++i)
        {
            _workers.emplace_back([this] {
                for (;;)
                {
                    Task task;

                    {
                        std::unique_lock<std::mutex> lockGuard(_taskMutex);
                        _taskCondition.wait(lockGuard, [this] {
                            return _tasks.empty() == false || _continue == false;
                        });
                        if (_continue == false)
                        {
                            break;
                        }
                        if (!_tasks.empty())
                        {
                            task = std::move(_tasks.front());
                            _tasks.pop();
                        }
                    }

                    if (task)
                    {
                        _executingCount++;
                        task(this);
                        _executingCount--;
                    }
                }
            });
        }
    }
    ThreadPool::ProcessResult ThreadPool::processTask()
	{
		Task task;

		{
			std::unique_lock<std::mutex> lockGuard( _taskMutex );
			if( !_tasks.empty() )
			{
				task = std::move( _tasks.front() );
				_tasks.pop();
			}
		}

		if( task )
		{
			task( this );
            return ProcessResult::Consumed;
		}
        return ProcessResult::NoTask;
	}
#endif
}

