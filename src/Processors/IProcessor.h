#pragma once

#include <Common/MemorySpillScheduler.h>
#include <Common/Stopwatch.h>

#include <list>
#include <memory>
#include <vector>
#include <fmt/format.h>

class EventCounter;


namespace DB
{

class InputPort;
class OutputPort;
using InputPorts = std::list<InputPort>;
using OutputPorts = std::list<OutputPort>;

class IQueryPlanStep;

struct StorageLimits;
using StorageLimitsList = std::list<StorageLimits>;

class RowsBeforeStepCounter;
using RowsBeforeStepCounterPtr = std::shared_ptr<RowsBeforeStepCounter>;

class IProcessor;
using ProcessorPtr = std::shared_ptr<IProcessor>;
using Processors = std::vector<ProcessorPtr>;

/** Processor is an element (low level building block) of a query execution pipeline.
  * It has zero or more input ports and zero or more output ports.
  * 处理器是查询执行管道中的一个元素（低级构建块）。
  * 它有零个或多个输入端口和零个或多个输出端口。
  *
  * Blocks of data are transferred over ports.
  * 数据块通过端口传输。
  * Each port has fixed structure: names and types of columns and values of constants.
  * 每个端口都有一个固定的结构：列的名称和类型以及常量值。
  *
  * Processors may pull data from input ports, do some processing and push data to output ports.
  * 处理器可能从输入端口拉取数据，进行一些处理，并将数据推送到输出端口。
  * Processor may indicate that it requires input data to proceed and indicate that it needs data from some ports.
  * 处理器可能指示它需要输入数据才能继续，并指示它需要来自某些端口的数据。
  *
  * Synchronous work must only use CPU - don't do any sleep, IO wait, network wait.
  * 同步工作必须只使用CPU - 不要进行任何睡眠、IO等待或网络等待。
  *
  * Processor may want to do work asynchronously (example: fetch data from remote server)
  *  - in this case it will initiate background job and allow to subscribe to it.
  * 处理器可能想要异步工作（例如：从远程服务器获取数据）
  *  - 在这种情况下，它将启动后台作业并允许订阅它。
  *
  * Processor may throw an exception to indicate some runtime error.
  * 处理器可能抛出异常以指示某些运行时错误。
  *
  * Different ports may have different structure. For example, ports may correspond to different resultsets
  *  or semantically different parts of result.
  * 不同的端口可能具有不同的结构。例如，端口可能对应于不同的结果集或语义不同的结果部分。
  *
  * Processor may modify its ports (create another processors and connect to them) on the fly.
  * Example: first execute the subquery; on basis of subquery result
  *  determine how to execute the rest of query and build the corresponding pipeline.
  * 处理器可以在运行时修改其端口（创建另一个处理器并连接到它们）。
  * 示例：首先执行子查询；基于子查询结果确定如何执行查询的其余部分，并构建相应的管道。
  *
  * Processor may simply wait for another processor to execute without transferring any data from it.
  * 处理器可能简单地等待另一个处理器执行，而不从它传输任何数据。
  * For this purpose it should connect its input port to another processor, and indicate need of data.
  * 为此，它应该将其输入端口连接到另一个处理器，并指示需要数据。
  *
  * Examples:
  * 示例：
  *
  * Source. Has no input ports and single output port. Generates data itself and pushes it to its output port.
  * 源。没有输入端口和单个输出端口。自己生成数据并推送到其输出端口。
  *
  * Sink. Has single input port and no output ports. Consumes data that was passed to its input port.
  * 接收器。有一个输入端口和没有输出端口。消耗传递给其输入端口的数据。
  * Empty source. Immediately says that data on its output port is finished.
  * 空源。立即说其输出端口上的数据已结束。
  *
  * Null sink. Consumes data and does nothing.
  * 空接收器。消耗数据并什么都不做。
  *
  * Simple transformation. Has single input and single output port. Pulls data, transforms it and pushes to output port.
  * Example: expression calculator.
  * 简单转换。有一个输入端口和一个输出端口。拉取数据，转换它，并推送到输出端口。
  * 示例：表达式计算器。
  *
  * Squashing or filtering transformation. Pulls data, possibly accumulates it, and sometimes pushes it to output port.
  * Examples: DISTINCT, WHERE, squashing of blocks for INSERT SELECT.
  * 压缩或过滤转换。拉取数据，可能累积它，有时推送到输出端口。
  * 示例：DISTINCT, WHERE, 为INSERT SELECT压缩块。
  *
  * Accumulating transformation. Pulls and accumulates all data from input until it it exhausted, then pushes data to output port.
  * Examples: ORDER BY, GROUP BY.
  * 累积转换。拉取并累积所有数据，直到它耗尽，然后推送到输出端口。
  * 示例：ORDER BY, GROUP BY.
  *
  * Limiting transformation. Pulls data from input and passes to output.
  * When there was enough data, says that it doesn't need data on its input and that data on its output port is finished.
  * 限制转换。从输入中拉取数据，并推送到输出端口。
  * 当有足够的数据时，说它不需要输入端口上的数据，并且输出端口上的数据已结束。
  *
  * Resize. Has arbitrary number of inputs and arbitrary number of outputs.
  * Pulls data from whatever ready input and pushes it to randomly chosen free output.
  * Examples:
  * Union - merge data from number of inputs to one output in arbitrary order.
  * Split - read data from one input and pass it to arbitrary output.
  * 示例：
  * Union - 将来自多个输入的数据合并到一个输出中，以任意顺序。
  * Split - 从单个输入中读取数据，并将其传递到任意输出。
  *
  * Concat. Has many inputs and only one output. Pulls all data from first input until it is exhausted,
  *  then all data from second input, etc. and pushes all data to output.
  * 连接。有许多输入和一个输出。从第一个输入中拉取所有数据，直到它耗尽，然后从第二个输入中拉取所有数据，等等，并将所有数据推送到输出。
  *
  * Ordered merge. Has many inputs but only one output. Pulls data from selected input in specific order, merges and pushes it to output.
  * 有序合并。有许多输入但只有一个输出。以特定顺序从选定的输入中拉取数据，合并它，并推送到输出。
  *
  * Fork. Has one input and many outputs. Pulls data from input and copies it to all outputs.
  * Used to process multiple queries with common source of data.
  * 分支。有一个输入和许多输出。从输入中拉取数据，并复制它到所有输出。
  * 用于处理具有共同数据源的多个查询。
  *
  * Select. Has one or multiple inputs and one output.
  * Read blocks from inputs and check that blocks on inputs are "parallel": correspond to each other in number of rows.
  * Construct a new block by selecting some subset (or all) of columns from inputs.
  * Example: collect columns - function arguments before function execution.
  * 选择。有一个或多个输入和一个输出。从输入中拉取数据，并检查输入上的数据是“并行”的：对应于每个行的数量。
  * 构造一个新块，从输入中选择一些子集（或全部）列。
  * 示例：收集列 - 函数执行前的函数参数。
  *
  * TODO Processors may carry algebraic properties about transformations they do.
  * For example, that processor doesn't change number of rows; doesn't change order of rows, doesn't change the set of rows, etc.
  * 处理器可能携带关于它们执行的转换的代数性质。
  * 例如，处理器不改变行数；不改变行顺序，不改变行集等。
  *
  * TODO Ports may carry algebraic properties about streams of data.
  * For example, that data comes ordered by specific key; or grouped by specific key; or have unique values of specific key.
  * And also simple properties, including lower and upper bound on number of rows.
  * 端口可能携带关于数据流的代数性质。
  * 例如，数据按特定键排序；或按特定键分组；或具有特定键的唯一值。
  * 以及简单属性，包括行数的上限和下限。
  *
  * TODO Processor should have declarative representation, that is able to be serialized and parsed.
  * Example: read_from_merge_tree(database, table, Columns(a, b, c), Piece(0, 10), Parts(Part('name', MarkRanges(MarkRange(0, 100), ...)), ...))
  * It's reasonable to have an intermediate language for declaration of pipelines.
  * 处理器应该有一个声明性的表示，能够被序列化和解析。
  * 示例：read_from_merge_tree(database, table, Columns(a, b, c), Piece(0, 10), Parts(Part('name', MarkRanges(MarkRange(0, 100), ...)), ...))
  * 有一个中间语言来声明管道是合理的。
  *
  * TODO Processor with all its parameters should represent "pure" function on streams of data from its input ports.
  * It's in question, what kind of "pure" function do we mean.
  * For example, data streams are considered equal up to order unless ordering properties are stated explicitly.
  * Another example: we should support the notion of "arbitrary N-th of M substream" of full stream of data.
  * 处理器应该代表其输入端口上数据流的“纯”函数。
  * 这取决于我们所说的“纯”函数是什么。
  * 例如，数据流被认为是相等的，除非显式声明了排序属性。
  * 另一个例子：我们应该支持“任意N-th的M子流”的概念。
  */

class IProcessor
{
protected:
    InputPorts inputs;
    OutputPorts outputs;

public:
    IProcessor();

    IProcessor(InputPorts inputs_, OutputPorts outputs_);

    virtual String getName() const = 0;

    String getUniqID() const { return fmt::format("{}_{}", getName(), processor_index); }

    enum class Status : uint8_t
    {
        /// Processor needs some data at its inputs to proceed.
        /// You need to run another processor to generate required input and then call 'prepare' again.
        /// 处理器需要一些数据在其输入端口上才能继续。
        /// 您需要运行另一个处理器来生成所需的数据，然后再次调用'prepare'。
        NeedData,

        /// Processor cannot proceed because output port is full or not isNeeded().
        /// You need to transfer data from output port to the input port of another processor and then call 'prepare' again.
        /// 处理器无法继续，因为输出端口已满或未isNeeded()。
        /// 您需要将数据从输出端口传输到另一个处理器的输入端口，然后再次调用'prepare'。
        PortFull,

        /// All work is done (all data is processed or all output are closed), nothing more to do.
        /// 所有工作都已完成（所有数据都已处理或所有输出都已关闭），没有更多要做的工作。
        Finished,

        /// No one needs data on output ports.
        /// 没有人需要输出端口上的数据。
        /// Unneeded,

        /// You may call 'work' method and processor will do some work synchronously.
        /// 您可以调用'work'方法，处理器将同步执行一些工作。
        Ready,

        /// You may call 'schedule' method and processor will return a descriptor.
        /// You need to poll this descriptor and call work() afterwards.
        /// 您可以调用'schedule'方法，处理器将返回一个描述符。
        /// 您需要轮询此描述符，并在之后调用work()。
        Async,

        /// Processor wants to add other processors to pipeline.
        /// New processors must be obtained by expandPipeline() call.
        /// 处理器想要添加其他处理器到管道。
        /// 新处理器必须通过expandPipeline()调用获得。
        ExpandPipeline,
    };

    static std::string statusToName(std::optional<Status> status);

    /** Method 'prepare' is responsible for all cheap ("instantaneous": O(1) of data volume, no wait) calculations.
      *
      * It may access input and output ports,
      *  indicate the need for work by another processor by returning NeedData or PortFull,
      *  or indicate the absence of work by returning Finished or Unneeded,
      *  it may pull data from input ports and push data to output ports.
      *
      * The method is not thread-safe and must be called from a single thread in one moment of time,
      *  even for different connected processors.
      *
      * Instead of all long work (CPU calculations or waiting) it should just prepare all required data and return Ready or Async.
      *
      * Thread safety and parallel execution:
      * - no methods (prepare, work, schedule) of single object can be executed in parallel;
      * - method 'work' can be executed in parallel for different objects, even for connected processors;
      * - method 'prepare' cannot be executed in parallel even for different objects,
      *   if they are connected (including indirectly) to each other by their ports;
      */
    virtual Status prepare();

    using PortNumbers = std::vector<UInt64>;

    /// Optimization for prepare in case we know ports were updated.
    virtual Status prepare(const PortNumbers & /*updated_input_ports*/, const PortNumbers & /*updated_output_ports*/) { return prepare(); }

    /** You may call this method if 'prepare' returned Ready.
      * This method cannot access any ports. It should use only data that was prepared by 'prepare' method.
      * 您可以调用此方法，如果'prepare'返回Ready。
      * 此方法不能访问任何端口。它应该只使用'prepare'方法准备的数据。
      *
      * Method work can be executed in parallel for different processors.
      * 方法work可以并行执行不同的处理器。
      */
    virtual void work();

    /** Executor must call this method when 'prepare' returned Async.
      * This method cannot access any ports. It should use only data that was prepared by 'prepare' method.
      * 您可以调用此方法，如果'prepare'返回Async。
      * 此方法不能访问任何端口。它应该只使用'prepare'方法准备的数据。
      *
      * This method should instantly return epollable file descriptor which will be readable when asynchronous job is done.
      * 此方法应该立即返回一个可读的epoll文件描述符，当异步作业完成时，该文件描述符可读。
      * When descriptor is readable, method `work` is called to continue data processing.
      * 当描述符可读时，方法`work`被调用来继续数据处理。
      *
      * NOTE: it would be more logical to let `work()` return ASYNC status instead of prepare. 
      * This will get prepare() -> work() -> schedule() -> work() -> schedule() -> .. -> work() -> prepare()
      * chain instead of
      * 链而不是
      * prepare() -> work() -> prepare() -> schedule() -> work() -> prepare() -> schedule() -> .. -> work() -> prepare()
      *
      * It is expected that executor epoll using level-triggered notifications.
      * 预计执行器使用水平触发通知。
      * Read all available data from descriptor before returning ASYNC.
      * 在返回ASYNC之前，从描述符中读取所有可用的数据。
      */
    virtual int schedule();

    /* The method is called right after asynchronous job is done
     * 在异步作业完成后立即调用此方法。
     * i.e. when file descriptor returned by schedule() is readable.
     * 当schedule()返回的文件描述符可读时，调用此方法。
     *
     * The sequence of method calls:
     * ... prepare() -> schedule() -> onAsyncJobReady() -> work() ...
     * See also comment to schedule() method
     *
     * It allows doing some preprocessing immediately after asynchronous job is done.
     * The implementation should return control quickly, to avoid blocking another asynchronous completed jobs
     * created by the same pipeline.
     *
     * Example, scheduling tasks for remote workers (file descriptor in this case is a socket)
     * When the remote worker asks for the next task, doing it in onAsyncJobReady() we can provide it immediately.
     * Otherwise, the returning of the next task for the remote worker can be delayed by current work done in the pipeline
     * (by other processors), which will create unnecessary latency in query processing by remote workers
     */
    virtual void onAsyncJobReady() {}

    /** You must call this method if 'prepare' returned ExpandPipeline.
      * 您必须调用此方法，如果'prepare'返回ExpandPipeline。
      * This method cannot access any port, but it can create new ports for current processor.
      * 此方法不能访问任何端口，但它可以为当前处理器创建新的端口。
      *
      * Method should return set of new already connected processors.
      * 方法应该返回一组新的已连接的处理器。
      * All added processors must be connected only to each other or current processor.
      * 所有添加的处理器必须仅连接到彼此或当前处理器。
      *
      * Method can't remove or reconnect existing ports, move data from/to port or perform calculations.
      * 方法不能删除或重新连接现有端口，移动数据从/到端口或执行计算。
      * 'prepare' should be called again after expanding pipeline.
      */
    virtual Processors expandPipeline();

    /// In case if query was cancelled executor will wait till all processors finish their jobs.
    /// 如果查询被取消，执行器将等待所有处理器完成其作业。
    /// Generally, there is no reason to check this flag. However, it may be reasonable for long operations (e.g. i/o).
    /// 通常，没有理由检查此标志。然而，对于长时间操作（例如i/o），这可能是合理的。
    bool isCancelled() const { return is_cancelled.load(std::memory_order_acquire); }
    void cancel() noexcept;

    /// Additional method which is called in case if ports were updated while work() method.
    /// 在work()方法期间，如果端口被更新，则调用此方法。
    /// May be used to stop execution in rare cases.
    /// 可用于在罕见情况下停止执行。
    virtual void onUpdatePorts() {}

    virtual ~IProcessor() = default;

    auto & getInputs() { return inputs; }
    auto & getOutputs() { return outputs; }

    UInt64 getInputPortNumber(const InputPort * input_port) const;

    UInt64 getOutputPortNumber(const OutputPort * output_port) const;

    const auto & getInputs() const { return inputs; }
    const auto & getOutputs() const { return outputs; }

    /// Debug output.
    String debug() const;
    void dump() const;

    /// Used to print pipeline.
    /// 用于打印管道。
    void setDescription(const std::string & description_) { processor_description = description_; }
    const std::string & getDescription() const { return processor_description; }

    /// Helpers for pipeline executor.
    /// 用于管道执行器的辅助函数。
    void setStream(size_t value) { stream_number = value; }
    size_t getStream() const { return stream_number; }
    constexpr static size_t NO_STREAM = std::numeric_limits<size_t>::max();

    /// Step of QueryPlan from which processor was created.
    /// 查询计划中的步骤，从中创建处理器。
    void setQueryPlanStep(IQueryPlanStep * step, size_t group = 0);

    IQueryPlanStep * getQueryPlanStep() const { return query_plan_step; }
    const String & getStepUniqID() const { return step_uniq_id; }
    size_t getQueryPlanStepGroup() const { return query_plan_step_group; }
    const String & getPlanStepName() const { return plan_step_name; }
    const String & getPlanStepDescription() const { return plan_step_description; }

    uint64_t getElapsedNs() const { return elapsed_ns; }
    uint64_t getInputWaitElapsedNs() const { return input_wait_elapsed_ns; }
    uint64_t getOutputWaitElapsedNs() const { return output_wait_elapsed_ns; }

    struct ProcessorDataStats
    {
        size_t input_rows = 0;
        size_t input_bytes = 0;
        size_t output_rows = 0;
        size_t output_bytes = 0;
    };

    ProcessorDataStats getProcessorDataStats() const;

    struct ReadProgressCounters
    {
        uint64_t read_rows = 0;
        uint64_t read_bytes = 0;
        uint64_t total_rows_approx = 0;
        uint64_t total_bytes = 0;
    };

    struct ReadProgress
    {
        ReadProgressCounters counters;
        const StorageLimitsList & limits;
    };

    /// Set limits for current storage.
    /// 设置当前存储的限制。
    /// Different limits may be applied to different storages, we need to keep it per processor.
    /// 不同的限制可以应用于不同的存储，我们需要为每个处理器保持它。
    /// This method needs to be overridden only for sources.
    /// 此方法仅适用于源。
    virtual void setStorageLimits(const std::shared_ptr<const StorageLimitsList> & /*storage_limits*/) {}

    /// This method is called for every processor without input ports.
    /// 对于没有输入端口的每个处理器，调用此方法。
    /// Processor can return new progress for the last read operation.
    /// 处理器可以返回最后一次读取操作的新进度。
    /// You should zero internal counters in the call, in order to make in idempotent.
    /// 您应该在调用中将内部计数器归零，以使其幂等。
    virtual std::optional<ReadProgress> getReadProgress() { return std::nullopt; }

    /// Set rows_before_limit counter for current processor.
    /// 设置当前处理器的rows_before_limit计数器。
    /// This counter is used to calculate the number of rows right before any filtration of LimitTransform.
    /// 此计数器用于计算LimitTransform之前的行数。
    virtual void setRowsBeforeLimitCounter(RowsBeforeStepCounterPtr /* counter */) { }

    /// Set rows_before_aggregation counter for current processor.
    /// 设置当前处理器的rows_before_aggregation计数器。
    /// This counter is used to calculate the number of rows right before AggregatingTransform.
    /// 此计数器用于计算AggregatingTransform之前的行数。
    virtual void setRowsBeforeAggregationCounter(RowsBeforeStepCounterPtr /* counter */) { }

    /// Returns true if processor can spill memory to disk.
    /// 如果处理器可以将内存溢出到磁盘，则返回true。
    /// Aggregate, join and sort processors can be spillable.
    /// 聚合、连接和排序处理器可以是溢出的。
    /// For unspillable processors, the memory usage is not tracked.
    /// 对于不可溢出的处理器，内存使用情况不会被跟踪。
    inline bool isSpillable() const { return spillable; }

    virtual ProcessorMemoryStats getMemoryStats()
    {
        return {};
    }

    // If the in-memory data's size is not larger then bytes, it doesn't spill
    virtual bool spillOnSize(size_t /*bytes*/) { return false; }

protected:
    virtual void onCancel() noexcept {}

    std::atomic<bool> is_cancelled{false};
    bool spillable = false;

private:
    /// For:
    /// - elapsed_ns
    friend class ExecutionThreadContext;
    /// For
    /// - input_wait_elapsed_ns
    /// - output_wait_elapsed_ns
    friend class ExecutingGraph;

    std::string processor_description;

    /// For processors_profile_log
    uint64_t elapsed_ns = 0;
    Stopwatch input_wait_watch;
    uint64_t input_wait_elapsed_ns = 0;
    Stopwatch output_wait_watch;
    uint64_t output_wait_elapsed_ns = 0;

    size_t stream_number = NO_STREAM;

    IQueryPlanStep * query_plan_step = nullptr;
    String step_uniq_id;
    size_t query_plan_step_group = 0;

    size_t processor_index = 0;
    String plan_step_name;
    String plan_step_description;

};


}
