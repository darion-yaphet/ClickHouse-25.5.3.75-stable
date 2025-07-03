#pragma once

#include <Processors/Chunk.h>
#include <Processors/IProcessor.h>
#include <Processors/Port.h>

namespace DB
{

/** Has one input and one output.
  * 有一个输入和一个输出。
  * Simply pull a block from input, transform it, and push it to output.
  * 从输入中拉取一个块，转换它，并推送到输出。
  */
class ISimpleTransform : public IProcessor
{
protected:
    InputPort & input;
    OutputPort & output;

    Port::Data input_data;
    Port::Data output_data;
    bool has_input = false;
    bool has_output = false;
    bool no_more_data_needed = false;
    const bool skip_empty_chunks;

    /// Set input port NotNeeded after chunk was pulled.
    /// 设置输入端口在块被拉取后变为不可用。
    /// Input port will become needed again only after data was transformed.
    /// This allows to escape caching chunks in input port, which can lead to uneven data distribution.
    /// 输入端口只有在数据被转换后才会变为可用。
    /// 这允许避免在输入端口缓存块，这可能导致数据分布不均匀。
    bool set_input_not_needed_after_read = true;

    virtual void transform(Chunk & input_chunk, Chunk & output_chunk)
    {
        transform(input_chunk);
        output_chunk.swap(input_chunk);
    }

    /// 如果需要输入数据，则返回true。
    virtual bool needInputData() const { return true; }
    /// 停止读取数据。
    void stopReading() { no_more_data_needed = true; }

public:
    ISimpleTransform(Block input_header_, Block output_header_, bool skip_empty_chunks_);

    virtual void transform(Chunk &) = 0;

    Status prepare() override;
    void work() override;

    InputPort & getInputPort() { return input; }
    OutputPort & getOutputPort() { return output; }

    void setInputNotNeededAfterRead(bool value) { set_input_not_needed_after_read = value; }
};

}
