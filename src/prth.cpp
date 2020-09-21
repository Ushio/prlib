#include "prth.hpp"

namespace pr
{
	ThreadPool::ThreadPool(int nThreads, int64_t nReminedElement)
        :_nThreads(nThreads), _nReminedElement(nReminedElement)
    {
        _continue = true;
        _hasElement = 0 < _nReminedElement;

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
    void ThreadPool::enqueueFor(int64_t nExecute, std::function<void(int64_t, int64_t, ThreadPool *)> work)
    {
        {
            std::unique_lock<std::mutex> lockGuard(_taskMutex);
            int nSplit = _nThreads * 2;
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
                        task(this);
                    }
                }
            });
        }
    }
}

