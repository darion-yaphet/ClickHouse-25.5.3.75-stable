#pragma once

#include <Processors/Port.h>
#include <Processors/IProcessor.h>
#include <Common/SharedMutex.h>
#include <Common/AllocatorWithMemoryTracking.h>
#include <mutex>
#include <queue>
#include <stack>
#include <vector>


namespace DB
{

/// Graph of executing pipeline.
/// 执行管道图。
class ExecutingGraph
{
public:
    /// Edge represents connection between OutputPort and InputPort.
    /// For every connection, two edges are created: direct and backward (it is specified by backward flag).
    /// 边表示 OutputPort 和 InputPort 之间的连接。
    /// 对于每个连接，创建两个边：直接和反向（由 backward 标志指定）。
    struct Edge
    {
        Edge(uint64_t to_, bool backward_,
             uint64_t input_port_number_, uint64_t output_port_number_,
             std::vector<void *> * update_list)
            : to(to_), backward(backward_)
            , input_port_number(input_port_number_), output_port_number(output_port_number_)
        {
            update_info.update_list = update_list;
            update_info.id = this;
        }

        /// Processor id this edge points to.
        /// 此边指向的处理器ID。
        /// It is processor with output_port for direct edge or processor with input_port for backward.
        /// 对于直接边，它是具有输出端口的处理器；对于反向边，它是具有输入端口的处理器。
        uint64_t to = std::numeric_limits<uint64_t>::max();
        /// 是否是反向边。
        bool backward;
        /// Port numbers. They are same for direct and backward edges.
        uint64_t input_port_number;
        uint64_t output_port_number;

        /// Edge version is increased when port's state is changed (e.g. when data is pushed). See Port.h for details.
        /// To compare version with prev_version we can decide if neighbour processor need to be prepared.
        Port::UpdateInfo update_info;
    };

    /// Use std::list because new ports can be added to processor during execution.
    /// 使用 std::list 因为执行期间可以向处理器添加新端口。
    using Edges = std::list<Edge>;

    /// Small structure with context of executing job.
    /// 执行作业的小结构。
    struct ExecutionState
    {
        std::exception_ptr exception;
        std::function<void()> job;

        IProcessor * processor = nullptr;
        uint64_t processors_id = 0;

        /// Counters for profiling.
        uint64_t num_executed_jobs = 0;
        uint64_t execution_time_ns = 0;
        uint64_t preparation_time_ns = 0;
    };

    /// Status for processor.
    /// Can be owning or not. Owning means that executor who set this status can change node's data and nobody else can.
    /// 处理器状态。
    /// 可以是拥有或不拥有。拥有意味着设置此状态的执行器可以更改节点数据，没有人可以更改。
    enum class ExecStatus : uint8_t
    {
        /// 空闲。prepare 返回 NeedData 或 PortFull。非拥有。
        Idle,  /// prepare returned NeedData or PortFull. Non-owning. 
        /// 准备中。一些执行器正在准备处理器，或者处理器在任务队列中。拥有。
        Preparing,  /// some executor is preparing processor, or processor is in task_queue. Owning.
        /// 执行中。prepare 返回 Ready 并且任务正在执行。拥有。
        Executing,  /// prepare returned Ready and task is executing. Owning.
        /// 完成。prepare 返回 Finished。非拥有。
        Finished,  /// prepare returned Finished. Non-owning.
        /// 异步。prepare 返回 Async。拥有。
        Async  /// prepare returned Async. Owning.
    };

    /// Graph node. Represents single Processor.
    /// 图节点。表示单个处理器。
    struct Node
    {
        /// Processor and it's position in graph.
        /// 处理器和它在图中的位置。
        IProcessor * processor = nullptr;
        uint64_t processors_id = 0;

        /// Direct edges are for output ports, back edges are for input ports.
        /// 直接边用于输出端口，反向边用于输入端口。
        Edges direct_edges;
        Edges back_edges;

        /// Current status. It is accessed concurrently, using mutex.
        /// 当前状态。它被并发访问，使用互斥锁。
        ExecStatus status = ExecStatus::Idle;
        std::mutex status_mutex;

        /// Exception which happened after processor execution.
        /// 处理器执行后发生的异常。
        std::exception_ptr exception;

        /// Last state for profiling.
        /// 最后一次状态用于分析。
        std::optional<IProcessor::Status> last_processor_status;

        /// Ports which have changed their state since last processor->prepare() call.
        /// They changed when neighbour processors interact with connected ports.
        /// They will be used as arguments for next processor->prepare() (and will be cleaned after that).
        /// 在最后一次处理器->prepare()调用后，端口状态发生变化。
        /// 它们在相邻处理器交互连接端口时发生变化。
        /// 它们将用作下一个处理器->prepare()的参数（并在之后清理）。
        IProcessor::PortNumbers updated_input_ports;
        IProcessor::PortNumbers updated_output_ports;

        /// Ports that have changed their state during last processor->prepare() call.
        /// We use this data to fill updated_input_ports and updated_output_ports for neighbour nodes.
        /// This containers are temporary, and used only after processor->prepare() is called.
        /// They could have been local variables, but we need persistent storage for Port::UpdateInfo.
        Port::UpdateInfo::UpdateList post_updated_input_ports;
        Port::UpdateInfo::UpdateList post_updated_output_ports;

        /// Counters for profiling.
        uint64_t num_executed_jobs = 0;
        uint64_t execution_time_ns = 0;
        uint64_t preparation_time_ns = 0;

        Node(IProcessor * processor_, uint64_t processor_id)
            : processor(processor_), processors_id(processor_id)
        {
        }
    };

    /// This queue can grow a lot and lead to OOM. That is why we use non-default
    /// allocator for container which throws exceptions in operator new
    /// 这个队列可以增长很大，导致OOM。这就是为什么我们使用非默认的分配器来容器，它在operator new中抛出异常。
    using DequeWithMemoryTracker = std::deque<ExecutingGraph::Node *, AllocatorWithMemoryTracking<ExecutingGraph::Node *>>;
    using Queue = std::queue<ExecutingGraph::Node *, DequeWithMemoryTracker>;

    /// 节点指针。
    using NodePtr = std::unique_ptr<Node>;
    /// 节点向量。
    using Nodes = std::vector<NodePtr>;
    Nodes nodes;

    /// IProcessor * -> processors_id (position in graph)
    /// 处理器映射。
    using ProcessorsMap = std::unordered_map<const IProcessor *, uint64_t>;
    ProcessorsMap processors_map;

    explicit ExecutingGraph(std::shared_ptr<Processors> processors_, bool profile_processors_);

    const Processors & getProcessors() const { return *processors; }

    /// Traverse graph the first time to update all the childless nodes.
    void initializeExecution(Queue & queue, Queue & async_queue);

    enum class UpdateNodeStatus
    {
        Done,
        Exception,
        Cancelled,
    };

    /// Update processor with pid number (call IProcessor::prepare).
    /// Check parents and children of current processor and push them to stacks if they also need to be updated.
    /// If processor wants to be expanded, lock will be upgraded to get write access to pipeline.
    UpdateNodeStatus updateNode(uint64_t pid, Queue & queue, Queue & async_queue);

    void cancel(bool cancel_all_processors = true);

private:
    /// Add single edge to edges list. Check processor is known.
    Edge & addEdge(Edges & edges, Edge edge, const IProcessor * from, const IProcessor * to);

    /// Append new edges for node. It is called for new node or when new port were added after ExpandPipeline.
    /// Returns true if new edge was added.
    bool addEdges(uint64_t node);

    /// Update graph after processor (pid) returned ExpandPipeline status.
    /// All new nodes and nodes with updated ports are pushed into stack.
    UpdateNodeStatus expandPipeline(std::stack<uint64_t> & stack, uint64_t pid);

    std::shared_ptr<Processors> processors;
    std::vector<bool> source_processors;
    std::mutex processors_mutex;

    SharedMutex nodes_mutex;

    const bool profile_processors;
    bool cancelled = false;
};

}
