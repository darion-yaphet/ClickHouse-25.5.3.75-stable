#pragma once

#include <Processors/Executors/ExecutionThreadContext.h>
#include <Processors/Executors/PollingQueue.h>
#include <Processors/Executors/ThreadsQueue.h>
#include <Processors/Executors/TasksQueue.h>
#include <stack>

namespace DB
{

/// Manage tasks which are ready for execution. Used in PipelineExecutor.
/// 管理准备执行的任务。用于PipelineExecutor。
class ExecutorTasks
{
    /// If query is finished (or cancelled).
    /// 如果查询已结束（或已取消）。
    std::atomic_bool finished = false;

    /// Contexts for every executing thread.
    /// 每个执行线程的上下文。
    std::vector<std::unique_ptr<ExecutionThreadContext>> executor_contexts;
    /// This mutex protects only executor_contexts vector. Needed to avoid race between init() and finish().
    /// 这个互斥锁只保护executor_contexts向量。需要避免init()和finish()之间的竞争。
    std::mutex executor_contexts_mutex;

    /// Common mutex for all the following fields.
    /// 所有以下字段的通用互斥锁。
    std::mutex mutex;

    /// Queue with pointers to tasks. Each thread will concurrently read from it until finished flag is set.
    /// Stores processors need to be prepared. Preparing status is already set for them.
    /// 存储需要准备的处理器。它们的准备状态已经设置。
    TaskQueue<ExecutingGraph::Node> task_queue;

    /// Async tasks should be processed with higher priority, but also require task stealing logic.
    /// So we have a separate queue specifically for them.
    /// 异步任务需要更高的优先级，但还需要任务窃取逻辑。
    /// 所以我们有一个专门的队列专门用于它们。
    TaskQueue<ExecutingGraph::Node> fast_task_queue;
    std::atomic_bool has_fast_tasks = false; // Required only to enable local task optimization

    /// Queue which stores tasks where processors returned Async status after prepare.
    /// If multiple threads are used, main thread will wait for async tasks.
    /// For single thread, will wait for async tasks only when task_queue is empty.
    /// 存储异步任务的队列。
    /// 如果多个线程被使用，主线程将等待异步任务。
    /// 对于单线程，只有在task_queue为空时才会等待异步任务。
    PollingQueue async_task_queue;

    /// Maximum amount of threads. Constant after initialization, based on `max_threads` setting.
    /// 最大线程数。初始化后是常量，基于`max_threads`设置。
    size_t num_threads = 0;

    /// Started thread count (allocated by `ConcurrencyControl`). Can increase during execution up to `num_threads`.
    /// 已启动线程数（由`ConcurrencyControl`分配）。在执行期间可以增加到`num_threads`。
    size_t use_threads = 0;

    /// Number of idle threads, changed with threads_queue.size().
    /// 空闲线程数，随着threads_queue.size()的变化而变化。
    std::atomic_size_t idle_threads = 0;

    /// A set of currently waiting threads.
    /// 当前等待的线
    ThreadsQueue threads_queue;

    /// Threshold found by rolling dice.
    /// 通过掷骰子找到的阈值。
    const static size_t TOO_MANY_IDLE_THRESHOLD = 4;

public:
    using Stack = std::stack<UInt64>;
    /// This queue can grow a lot and lead to OOM. That is why we use non-default
    /// allocator for container which throws exceptions in operator new
    /// 这个队列可以增长很大，导致OOM。
    /// 这就是为什么我们使用非默认的分配器来容器，它在operator new中抛出异常。
    using DequeWithMemoryTracker = std::deque<ExecutingGraph::Node *, AllocatorWithMemoryTracking<ExecutingGraph::Node *>>;
    using Queue = std::queue<ExecutingGraph::Node *, DequeWithMemoryTracker>;

    void finish();
    bool isFinished() const { return finished; }

    void rethrowFirstThreadException();

    void tryWakeUpAnyOtherThreadWithTasks(ExecutionThreadContext & self, std::unique_lock<std::mutex> & lock);
    void tryWakeUpAnyOtherThreadWithTasksInQueue(ExecutionThreadContext & self, TaskQueue<ExecutingGraph::Node> & queue, std::unique_lock<std::mutex> & lock);

    /// It sets the task for specified thread `context`.
    /// If task was succeessfully found, one thread is woken up to process the remaining tasks.
    /// If there is no ready task yet, it blocks.
    /// If there are no more tasks, it finishes execution.
    /// Task priorities:
    ///   0. For num_threads == 1 we check async_task_queue directly
    ///   1. Async tasks from fast_task_queue for specified thread
    ///   2. Async tasks from fast_task_queue for other threads
    ///   3. Regular tasks from task_queue for specified thread
    ///   4. Regular tasks from task_queue for other threads
    void tryGetTask(ExecutionThreadContext & context);

    // Adds regular tasks from `queue` and async tasks from `async_queue` into queues for specified thread `context`.
    /// 将常规任务从`queue`和异步任务从`async_queue`添加到指定线程`context`的队列中。
    // Local task optimization: the first regular task could be placed directly into thread to be executed next.
    /// 本地任务优化：第一个常规任务可以直接放置到线程中，以便下一个执行。
    // For async tasks proessor->schedule() is called.
    /// 对于异步任务，调用processor->schedule()。
    // If non-local tasks were added, wake up one thread to process them.
    /// 如果添加了非本地任务，唤醒一个线程来处理它们。
    void pushTasks(Queue & queue, Queue & async_queue, ExecutionThreadContext & context);

    void init(size_t num_threads_, size_t use_threads_, bool profile_processors, bool trace_processors, ReadProgressCallback * callback);
    void fill(Queue & queue, Queue & async_queue);
    void upscale(size_t use_threads_);

    void processAsyncTasks();

    bool shouldSpawn() const { return idle_threads <= TOO_MANY_IDLE_THRESHOLD; }

    ExecutionThreadContext & getThreadContext(size_t thread_num) { return *executor_contexts[thread_num]; }
};

}
