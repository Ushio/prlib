#pragma once

#include <cassert>
#include <memory>
#include <atomic>
#include <future>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace pr
{
    // Manual task control

    class TaskGroup
    {
    public:
		TaskGroup() : _nReminedElement( 0 ), _condition( new std::condition_variable() )
        {
        }
        void addElements(int64_t nElements)
        {
            _nReminedElement.fetch_add(nElements);
        }
        void doneElements(int64_t nElements)
        {
            // fetch_sub could fire ~TaskGroup() destructor
            // so need to aquire instance of condition
			std::shared_ptr<std::condition_variable> condition = _condition;
            int64_t previous = _nReminedElement.fetch_sub( nElements );
            if( previous == nElements )
            {
				condition->notify_all();
            }
        }
        void waitForAllElementsToFinish()
        {
			// fetch_sub could fire ~TaskGroup() destructor
			// so need to aquire instance of condition
			std::shared_ptr<std::condition_variable> condition = _condition;
            std::unique_lock<std::mutex> lockGuard(_conditionMutex);
			condition->wait( lockGuard, [this] {
                return _nReminedElement.load() == 0;
            });
        }
        bool isFinished() const
        {
            return _nReminedElement.load() == 0;
        }
    private:
        std::atomic<int64_t> _nReminedElement;
        std::mutex _conditionMutex;
        std::shared_ptr<std::condition_variable> _condition;
    };

    class ThreadPool {
    public:
        using Task = std::function<void(void)>;

        ThreadPool(int nThreads);
        ~ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void enqueueTask(Task task);

        // work will be separated into splitLevel * nThreads tasks
        void enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t)> work);
        
        // process 1 task. you can use this even inside of Task function.
        // It's useful for nested enqueueFor()
        void processTask();

        int threadCount() const { return _nThreads; }
        float occupancy() const { return (float)_executingCount / _nThreads; };
    private:
        void launchThreads(int nThreads);
    private:
        int _nThreads = 0;
        std::future<void> _initThreads;
        bool _continue;
        std::vector<std::thread> _workers;
        std::queue<Task> _tasks;
        std::mutex _taskMutex;
        std::condition_variable _taskCondition;

        std::atomic<int32_t> _executingCount;
    };
}