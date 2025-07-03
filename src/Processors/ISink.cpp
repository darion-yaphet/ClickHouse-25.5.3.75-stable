#include <Processors/ISink.h>
#include <Processors/Port.h>

namespace DB
{

ISink::ISink(Block header)
    : IProcessor({std::move(header)}, {}), input(inputs.front())
{
}

ISink::Status ISink::prepare()
{
    /// 如果onStart()未被调用，则返回Ready状态。
    if (!was_on_start_called)
        return Status::Ready;

    /// 如果输入端口有数据，则返回Ready状态。
    if (has_input)
        return Status::Ready;

    /// 如果输入端口已结束，则返回Ready状态。
    if (input.isFinished())
    {
        if (!was_on_finish_called)
            return Status::Ready;

        return Status::Finished;
    }

    /// 设置输入端口为需要数据。
    input.setNeeded();
    /// 如果输入端口没有数据，则返回NeedData状态。
    if (!input.hasData())
        return Status::NeedData;

    current_chunk = input.pull(true);
    has_input = true;
    return Status::Ready;
}

void ISink::work()
{
    if (!was_on_start_called)
    {
        was_on_start_called = true;
        onStart();
    }
    else if (has_input)
    {
        has_input = false;
        consume(std::move(current_chunk));
    }
    else if (!was_on_finish_called)
    {
        was_on_finish_called = true;
        onFinish();
    }
}

}
