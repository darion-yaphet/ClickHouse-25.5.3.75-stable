#pragma once
#include <vector>
#include <queue>
#include <Common/Exception.h>

namespace DB
{
namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

/// A set of task queues for multithreaded processing.
/// 一组用于多线程处理的队列。
/// Every threads has its dedicated queue and uses it preferably.
/// 每个线程都有其专用的队列，并优先使用它。
/// When there are no tasks left in dedicated queue it steals tasks from other threads
/// 当专用队列中没有任务时，从其他线程窃取任务。
template <typename Task>
class TaskQueue
{
public:
    /// 初始化队列。
    void init(size_t num_threads) { queues.resize(num_threads); }

    /// Push a task into thread's dedicated queue
    /// 将任务推入线程的专用队列。
    void push(Task * task, size_t thread_num)
    {
        queues[thread_num].push(task);
        ++num_tasks;
    }

    /// Returns thread number to pop task from.
    /// 返回从哪个线程获取任务。
    /// First it check dedicated queue, and only if it is empty, it steal from other threads
    /// 首先检查专用队列，如果为空，则从其他线程窃取任务。
    size_t getAnyThreadWithTasks(size_t from_thread = 0)
    {
        if (num_tasks == 0)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "TaskQueue is empty");

        for (size_t i = 0; i < queues.size(); ++i)
        {
            if (!queues[from_thread].empty())
                return from_thread;

            ++from_thread;
            if (from_thread >= queues.size())
                from_thread = 0;
        }

        throw Exception(ErrorCodes::LOGICAL_ERROR, "TaskQueue is empty");
    }

    /// Pop a task from the specified queue or steal from others
    /// 从指定队列或从其他线列中窃取任务。
    Task * pop(size_t thread_num)
    {
        auto thread_with_tasks = getAnyThreadWithTasks(thread_num);

        Task * task = queues[thread_with_tasks].front();
        queues[thread_with_tasks].pop();

        --num_tasks;
        return task;
    }

    size_t size() const { return num_tasks; }
    bool empty() const { return num_tasks == 0; }

private:
    using Queue = std::queue<Task *>;
    std::vector<Queue> queues;
    size_t num_tasks = 0;
};

}
