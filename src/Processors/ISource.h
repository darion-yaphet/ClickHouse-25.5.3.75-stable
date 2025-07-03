#pragma once

#include <Core/Block.h>
#include <Processors/Chunk.h>
#include <Processors/IProcessor.h>
#include <Processors/Port.h>

#include <atomic>
#include <mutex>


namespace DB
{

class ISource : public IProcessor
{
private:
    /// 用于保护读取进度计数器的互斥锁。
    std::mutex read_progress_mutex;
    /// 用于记录读取进度计数器的结构。
    ReadProgressCounters read_progress;
    /// 用于记录读取进度是否已设置的布尔值。
    std::atomic_bool read_progress_was_set = false;
    /// 用于记录是否启用自动进度跟踪的布尔值。
    bool auto_progress;

protected:
    OutputPort & output;
    bool has_input = false;
    bool finished = false;
    bool got_exception = false;
    Port::Data current_chunk;

    std::shared_ptr<const StorageLimitsList> storage_limits;

    virtual Chunk generate();
    virtual std::optional<Chunk> tryGenerate();

    void progress(size_t read_rows, size_t read_bytes);

public:
    explicit ISource(Block header, bool enable_auto_progress = true);
    ~ISource() override;

    Status prepare() override;
    void work() override;

    OutputPort & getPort() { return output; }
    const OutputPort & getPort() const { return output; }

    void setStorageLimits(const std::shared_ptr<const StorageLimitsList> & storage_limits_) override;

    /// Default implementation for all the sources.
    std::optional<ReadProgress> getReadProgress() override;

    void addTotalRowsApprox(size_t value);
    void addTotalBytes(size_t value);
};

using SourcePtr = std::shared_ptr<ISource>;

}
