#pragma once

#include <Processors/IProcessor.h>
#include <Processors/Executors/ExecutorTasks.h>
#include <Common/EventCounter.h>
#include <Common/ThreadPool_fwd.h>
#include <Common/ISlotControl.h>
#include <Common/AllocatorWithMemoryTracking.h>

#include <deque>
#include <queue>
#include <memory>


namespace DB
{

class QueryStatus;
using QueryStatusPtr = std::shared_ptr<QueryStatus>;
class ExecutingGraph;
using ExecutingGraphPtr = std::unique_ptr<ExecutingGraph>;

class ReadProgressCallback;
using ReadProgressCallbackPtr = std::unique_ptr<ReadProgressCallback>;


/// Executes query pipeline.
/// 执行查询管道。
class PipelineExecutor
{
public:
    /// Get pipeline as a set of processors.
    /// 获取管道作为一组处理器。
    /// Processors should represent full graph. 
    /// 处理器应该代表完整的图。
    /// All ports must be connected, all connected nodes are mentioned in set.
    /// 所有端口必须连接，所有连接的节点都包含在集合中。
    /// Executor doesn't own processors, just stores reference.
    /// 执行器不拥有处理器，只是存储引用。
    /// During pipeline execution new processors can appear. They will be added to existing set.
    /// 在管道执行期间，可以出现新的处理器。它们将被添加到现有集合中。
    /// Explicit graph representation is built in constructor. Throws if graph is not correct.
    /// 显式图表示在构造函数中构建。如果图不正确，则抛出。
    explicit PipelineExecutor(std::shared_ptr<Processors> & processors, QueryStatusPtr elem);
    ~PipelineExecutor();

    /// Execute pipeline in multiple threads. Must be called once.
    /// 在多个线程中执行管道。必须调用一次。
    /// In case of exception during execution throws any occurred.
    /// 在执行期间，如果发生异常，则抛出任何异常。
    void execute(size_t num_threads, bool concurrency_control);

    /// Execute single step. Step will be stopped when yield_flag is true.
    /// 执行单个步骤。当yield_flag为true时，步骤将停止。
    /// Execution is happened in a single thread.
    /// 执行发生在单个线程中。
    /// Return true if execution should be continued.
    bool executeStep(std::atomic_bool * yield_flag = nullptr);

    const Processors & getProcessors() const;

    enum class ExecutionStatus
    {
        NotStarted,
        Executing,
        Finished,
        Exception,
        CancelledByUser,
        CancelledByTimeout,
    };

    /// Cancel execution. May be called from another thread.
    /// 取消执行。可以从另一个线程调用。
    void cancel() { cancel(ExecutionStatus::CancelledByUser); }

    ExecutionStatus getExecutionStatus() const { return execution_status.load(); }

    /// Cancel processors which only read data from source. May be called from another thread.
    /// 取消仅从源读取数据的处理器。可以从另一个线程调用。
    void cancelReading();

    /// Checks the query time limits (cancelled or timeout). 
    /// 检查查询时间限制（取消或超时）。
    /// Throws on cancellation or when time limit is reached and the query uses "break"
    /// 在取消或时间限制达到且查询使用"break"时抛出。
    bool checkTimeLimit();

    /// Same as checkTimeLimit but it never throws. It returns false on cancellation or time limit reached
    /// 与checkTimeLimit相同，但它从不抛出。它在取消或时间限制达到时返回false。
    [[nodiscard]] bool checkTimeLimitSoft();

    /// Set callback for read progress.
    /// It would be called every time when processor reports read progress.
    void setReadProgressCallback(ReadProgressCallbackPtr callback);

private:
    ExecutingGraphPtr graph;

    ExecutorTasks tasks;

    /// Concurrency control related
    SlotAllocationPtr cpu_slots;
    AcquiredSlotPtr single_thread_cpu_slot; // cpu slot for single-thread mode to work using executeStep()
    std::unique_ptr<ThreadPool> pool;
    std::atomic_size_t threads = 0;
    std::mutex spawn_mutex;

    /// Flag that checks that initializeExecution was called.
    bool is_execution_initialized = false;
    /// system.processors_profile_log
    bool profile_processors = false;
    /// system.opentelemetry_span_log
    bool trace_processors = false;

    std::atomic<ExecutionStatus> execution_status = ExecutionStatus::NotStarted;
    std::atomic_bool cancelled_reading = false;

    LoggerPtr log = getLogger("PipelineExecutor");

    /// Now it's used to check if query was killed.
    QueryStatusPtr process_list_element;

    ReadProgressCallbackPtr read_progress_callback;

    /// This queue can grow a lot and lead to OOM. That is why we use non-default
    /// allocator for container which throws exceptions in operator new
    using DequeWithMemoryTracker = std::deque<ExecutingGraph::Node *, AllocatorWithMemoryTracking<ExecutingGraph::Node *>>;
    using Queue = std::queue<ExecutingGraph::Node *, DequeWithMemoryTracker>;

    void initializeExecution(size_t num_threads, bool concurrency_control); /// Initialize executor contexts and task_queue.
    void finalizeExecution(); /// Check all processors are finished.
    void spawnThreads();
    void spawnThreadsImpl(AcquiredSlotPtr slot) TSA_REQUIRES(spawn_mutex);

    /// Methods connected to execution.
    void executeImpl(size_t num_threads, bool concurrency_control);
    void executeStepImpl(size_t thread_num, std::atomic_bool * yield_flag = nullptr);
    void executeSingleThread(size_t thread_num);
    void finish();
    void cancel(ExecutionStatus reason);

    /// If execution_status == from, change it to desired.
    bool tryUpdateExecutionStatus(ExecutionStatus expected, ExecutionStatus desired);

    String dumpPipeline() const;
};

using PipelineExecutorPtr = std::shared_ptr<PipelineExecutor>;

}
