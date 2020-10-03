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

#define ENABLE_IOCP 1

namespace pr
{
#if ENABLE_IOCP
    class ThreadPool {
    public:
        using Task = std::function<void(ThreadPool*)>;

        ThreadPool(int nThreads, int64_t nReminedElement = 0);
        ~ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void enqueueTask(Task task);

        // work will be separated into splitLevel * nThreads tasks
        void enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t, ThreadPool*)> work);

        // Manual task control
        void addElements(int64_t nElements);
        void doneElements(int64_t nElements);

        void waitForDoneElements();

        // process 1 task. you can use this even inside of Task function.
        // It's useful for nested enqueueFor()
        enum class ProcessResult
        {
            NoTask,
            Consumed,
        };
        ProcessResult processTask();

        int threadCount() const { return _nThreads; }
        float occupancy() const { return (float)_executingCount / _nThreads; };
    private:
        void launchThreads(int nThreads);
    private:
        int _nThreads = 0;
        std::future<void> _initThreads;
        std::vector<std::thread> _workers;
        void* _iocp;
        std::atomic<int64_t> _nReminedElement;

        std::mutex _statusMutex;
        std::condition_variable _statusCondition;
        bool _hasElement;

        std::atomic<int32_t> _executingCount;
    };
#else
    class ThreadPool {
    public:
        using Task = std::function<void(ThreadPool*)>;

        ThreadPool(int nThreads, int64_t nReminedElement = 0);
        ~ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void enqueueTask(Task task);

        // work will be separated into splitLevel * nThreads tasks
        void enqueueFor(int64_t nExecute, int64_t splitLevel, std::function<void(int64_t, int64_t, ThreadPool*)> work);
        
        // Manual task control
        void addElements(int64_t nElements);
        void doneElements(int64_t nElements);

        void waitForDoneElements();

        // process 1 task. you can use this even inside of Task function.
        // It's useful for nested enqueueFor()
        enum class ProcessResult
        {
            NoTask,
            Consumed,
        };
        ProcessResult processTask();

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
        std::atomic<int64_t> _nReminedElement;

        std::mutex _statusMutex;
        std::condition_variable _statusCondition;
        bool _hasElement;

        std::atomic<int32_t> _executingCount;
    };
#endif
}