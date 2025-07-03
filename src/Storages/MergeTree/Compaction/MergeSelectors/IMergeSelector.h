#pragma once

#include <Storages/MergeTree/Compaction/PartProperties.h>

namespace DB
{

/** Interface of algorithm to select data parts to merge
  *   (merge is also known as "compaction").
  *   合并（也称为“压缩”）数据部分的选择算法接口。
  * Following properties depend on it:
  *
  * 以下属性取决于它：
  *
  * 1. Number of data parts at some moment in time.
  *    在某个时刻的数据部分数量。
  *
  *    If parts are merged frequently, then data will be represented by lower number of parts, in average,
  *     but with cost of higher write amplification.
  *   如果部分合并频繁，则数据将由较低数量的部分表示，平均而言，但代价是更高的写入放大。
  *
  * 2. Write amplification ratio: how much times, on average, source data was written
  *     (during initial writes and followed merges). 
  *     写入放大比率：源数据被写入的次数（在初始写入和随后的合并期间）。
  *
  * Number of parallel merges are controlled outside of scope of this interface.
  * 并行合并的数量由该接口之外控制。
  */
class IMergeSelector
{
public:
    using RangeFilter = std::function<bool(PartsRangeView)>;

    /** Function could be called at any frequency and it must decide, should you do any merge at all.
      * If better not to do any merge, it returns empty result.
      * 函数可以以任何频率调用，并且必须决定是否进行任何合并。
      * 如果不需要合并，则返回空结果。
      */
    virtual PartsRange select(
        const PartsRanges & parts_ranges,
        size_t max_total_size_to_merge,
        RangeFilter range_filter) const = 0;

    virtual ~IMergeSelector() = default;
};

}
