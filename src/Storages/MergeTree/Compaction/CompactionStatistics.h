#pragma once

#include <Storages/MergeTree/Compaction/PartProperties.h>
#include <Storages/MergeTree/MergeTreeData.h>

namespace DB
{

namespace CompactionStatistics
{

/** Estimate approximate amount of disk space needed for merge or mutation. With a surplus.
  * 估计合并或突变所需的磁盘空间。
  */
UInt64 estimateNeededDiskSpace(const MergeTreeDataPartsVector & source_parts, const bool & account_for_deleted = false);

/** Estimate approximate amount of disk space needed to be free before schedule such merge.
  * 估计在调度合并之前所需的磁盘空间。
  */
UInt64 estimateAtLeastAvailableSpace(const PartsRange & range);

/** Get maximum total size of parts to do merge, at current moment of time.
  * It depends on number of free threads in background_pool and amount of free space in disk.
  * 获取当前时刻合并部分的最大总大小。
  * 它取决于后台池中的空闲线程数和磁盘中的空闲空间量。
  */
UInt64 getMaxSourcePartsSizeForMerge(const MergeTreeData & data);

/** For explicitly passed size of pool and number of used tasks.
  * This method could be used to calculate threshold depending on number of tasks in replication queue.
  * 对于显式传递的池大小和使用的任务数。
  * 此方法可用于计算依赖于复制队列中任务数的阈值。
  */
UInt64 getMaxSourcePartsSizeForMerge(const MergeTreeData & data, size_t max_count, size_t scheduled_tasks_count);

/** Same as above but with settings specification.
  * 与上面相同，但有设置规范。
  */
UInt64 getMaxSourcePartsSizeForMerge(
    size_t max_count,
    size_t scheduled_tasks_count,
    size_t max_unreserved_free_space,
    size_t size_lowering_threshold,
    size_t size_limit_at_min_pool_space,
    size_t size_limit_at_max_pool_space);

/** Get maximum total size of parts to do mutation, at current moment of time.
  * It depends only on amount of free space in disk.
  * 获取当前时刻突变部分的最大总大小。
  * 它只取决于磁盘中的空闲空间量。
  */
UInt64 getMaxSourcePartSizeForMutation(const MergeTreeData & data);

};

}
