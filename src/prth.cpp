#include "prth.hpp"

namespace pr
{
	ThreadPool::ThreadPool(int nThreads)
        :_nThreads(nThreads)
    {
        _continue = true;
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
    void ThreadPool::enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t)> work)
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

                _tasks.emplace([beg, end, work]() {
                    work(beg, end);
                });
            }
        }
        _taskCondition.notify_all();
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
                        task();
                        _executingCount--;
                    }
                }
            });
        }
    }
    void ThreadPool::processTask()
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
			task( );
		}
        else
        {
            std::this_thread::yield();
        }
	}
}

