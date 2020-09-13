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
    class ThreadPool {
    public:
        using Task = std::function<void(ThreadPool*)>;

        ThreadPool(int nThreads, int64_t nReminedElement = 0);
        ~ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void enqueueTask(Task task);
        void enqueueFor(int64_t nExecute, std::function<void(int64_t, int64_t, ThreadPool*)> work);
        void addElements(int64_t nElements);
        void doneElements(int64_t nElements);
        void waitForDoneElements();
    private:
        void launchThreads(int nThreads);
    private:
        int _nThreads = 0;
        std::future<void> _initThreads;
        bool _continue;
        std::vector< std::thread > _workers;
        std::queue< Task > _tasks;
        std::mutex _taskMutex;
        std::condition_variable _taskCondition;
        std::atomic<int64_t> _nReminedElement;

        std::mutex _statusMutex;
        std::condition_variable _statusCondition;
        bool _hasElement;
    };
}