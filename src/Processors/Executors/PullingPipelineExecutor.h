#pragma once
#include <memory>
#include <atomic>

namespace DB
{

class Block;
class Chunk;
class QueryPipeline;
class PipelineExecutor;
class PullingOutputFormat;
struct ProfileInfo;

using PipelineExecutorPtr = std::shared_ptr<PipelineExecutor>;

/// Pulling executor for QueryPipeline. Always execute pipeline in single thread.
/// 用于QueryPipeline的拉取执行器。始终在单线程中执行管道。
/// Typical usage is:
/// 典型用法是：
/// PullingPipelineExecutor executor(query_pipeline);
/// while (executor.pull(chunk))
///     ... process chunk ...
/// 
/// 如果需要多线程执行，请使用PipelineExecutor。
class PullingPipelineExecutor
{
public:
    explicit PullingPipelineExecutor(QueryPipeline & pipeline_);
    ~PullingPipelineExecutor();

    /// Get structure of returned block or chunk.
    /// 获取返回的块或块的结构。
    const Block & getHeader() const;

    /// Methods return false if query is finished.
    /// 如果查询已结束，则返回false。
    /// You can use any pull method.
    bool pull(Chunk & chunk);
    bool pull(Block & block);

    /// Stop execution. It is not necessary, but helps to stop execution before executor is destroyed.
    /// 停止执行。这不是必需的，但在执行器销毁之前停止执行很有帮助。
    void cancel();

    /// Get totals and extremes. Returns empty chunk if doesn't have any.
    /// 获取总计和极值。如果没有，则返回空块。
    Chunk getTotals();
    Chunk getExtremes();

    /// Get totals and extremes. Returns empty chunk if doesn't have any.
    /// 获取总计和极值。如果没有，则返回空块。
    Block getTotalsBlock();
    Block getExtremesBlock();

    /// Get query profile info.
    /// 获取查询配置文件信息。
    ProfileInfo & getProfileInfo();

private:
    std::atomic_bool has_data_flag = false;
    QueryPipeline & pipeline;
    std::shared_ptr<PullingOutputFormat> pulling_format;
    PipelineExecutorPtr executor;
};

}
