#pragma once

#include <Interpreters/Context_fwd.h>
#include <Interpreters/IJoin.h>
#include <Interpreters/TemporaryDataOnDisk.h>

#include <Core/Block.h>
#include <Core/Block_fwd.h>

#include <Common/MultiVersion.h>
#include <Common/SharedMutex.h>

#include <mutex>

namespace DB
{
class TableJoin;
class HashJoin;

/**
 * Efficient and highly parallel implementation of external memory JOIN based on HashJoin.
 * Supports most of the JOIN modes, except CROSS and ASOF.
 * 高效的并行实现基于HashJoin的外部内存JOIN。
 * 支持大多数JOIN模式，除了CROSS和ASOF。
 *
 * The joining algorithm consists of three stages:
 *
 * 1) During the first stage we accumulate blocks of the right table via @addBlockToJoin.
 * Each input block is split into multiple buckets based on the hash of the row join keys.
 * The first bucket is added to the in-memory HashJoin, and the remaining buckets are written to disk for further processing.
 * When the size of HashJoin exceeds the limits, we double the number of buckets.
 * There can be multiple threads calling addBlockToJoin, just like @ConcurrentHashJoin.
 *
 * 1) 在第一阶段，我们通过@addBlockToJoin累积右表的块。
 * 每个输入块根据行连接键的哈希值拆分为多个桶。
 * 第一个桶被添加到内存中的HashJoin中，其余的桶被写入磁盘以进一步处理。
 * 当HashJoin的大小超过限制时，我们增加桶的数量。
 * 可以有多个线程调用addBlockToJoin，就像@ConcurrentHashJoin一样。
 *
 * 2) At the second stage we process left table blocks via @joinBlock.
 * Again, each input block is split into multiple buckets by hash.
 * The first bucket is joined in-memory via HashJoin::joinBlock, and the remaining buckets are written to the disk.
 *
 * 2) 在第二阶段，我们通过@joinBlock处理左表的块。
 * 同样，每个输入块根据行连接键的哈希值拆分为多个桶。
 * 第一个桶通过HashJoin::joinBlock在内存中加入，其余的桶被写入磁盘。
 *
 * 3) When the last thread reading left table block finishes, the last stage begins.
 * Each @DelayedJoinedBlocksTransform calls repeatedly @getDelayedBlocks until there are no more unfinished buckets left.
 * Inside @getDelayedBlocks we select the next unprocessed bucket, load right table blocks from disk into in-memory HashJoin,
 * And then join them with left table blocks.
 *
 * 3) 当最后一个读取左表块的线程完成时，最后一个阶段开始。
 * 每个@DelayedJoinedBlocksTransform重复调用@getDelayedBlocks，直到没有未完成的桶为止。
 * 在@getDelayedBlocks中，我们选择下一个未处理的桶，将右表块从磁盘加载到内存中的HashJoin中，
 * 然后与左表块加入。
 *
 * After joining the left table blocks, we can load non-joined rows from the right table for RIGHT/FULL JOINs.
 * Note that non-joined rows are processed in multiple threads, unlike HashJoin/ConcurrentHashJoin/MergeJoin.
 *
 * 在加入左表块后，我们可以从右表加载非连接行用于RIGHT/FULL JOIN。
 * 注意，非连接行在多个线程中处理，与HashJoin/ConcurrentHashJoin/MergeJoin不同。
 */
class GraceHashJoin final : public IJoin
{
    class FileBucket;
    class DelayedBlocks;

    using InMemoryJoinPtr = std::shared_ptr<HashJoin>;

public:
    using BucketPtr = std::shared_ptr<FileBucket>;
    using Buckets = std::vector<BucketPtr>;

    GraceHashJoin(
        size_t initial_num_buckets_,
        size_t max_num_buckets_,
        std::shared_ptr<TableJoin> table_join_,
        const Block & left_sample_block_, const Block & right_sample_block_,
        TemporaryDataOnDiskScopePtr tmp_data_,
        bool any_take_last_row_ = false);

    ~GraceHashJoin() override;

    std::string getName() const override { return "GraceHashJoin"; }
    const TableJoin & getTableJoin() const override { return *table_join; }

    void initialize(const Block & sample_block) override;

    bool addBlockToJoin(const Block & block, bool check_limits) override;
    void checkTypesOfKeys(const Block & block) const override;
    void joinBlock(Block & block, std::shared_ptr<ExtraBlock> & not_processed) override;

    void setTotals(const Block & block) override;

    size_t getTotalRowCount() const override;
    size_t getTotalByteCount() const override;
    bool alwaysReturnsEmptySet() const override;

    bool supportParallelJoin() const override { return true; }
    bool supportTotals() const override { return false; }

    IBlocksStreamPtr
    getNonJoinedBlocks(const Block & left_sample_block_, const Block & result_sample_block_, UInt64 max_block_size) const override;

    /// Open iterator over joined blocks.
    /// Must be called after all @joinBlock calls.
    IBlocksStreamPtr getDelayedBlocks() override;
    bool hasDelayedBlocks() const override { return true; }
    bool rightTableCanBeReranged() const override;
    void tryRerangeRightTableData() override;

    void onBuildPhaseFinish() override;

    static bool isSupported(const std::shared_ptr<TableJoin> & table_join);

    void forceSpill() { force_spill = true; }

private:
    void initBuckets();
    /// Create empty join for in-memory processing.
    InMemoryJoinPtr makeInMemoryJoin(const String & bucket_id, size_t reserve_num = 0);

    /// Add right table block to the @join. Calls @rehash on overflow.
    void addBlockToJoinImpl(Block block);

    /// Check that join satisfies limits on rows/bytes in table_join.
    bool hasMemoryOverflow(size_t total_rows, size_t total_bytes) const;
    bool hasMemoryOverflow(const InMemoryJoinPtr & hash_join_) const;
    bool hasMemoryOverflow(const BlocksList & blocks) const;

    /// Add bucket_count new buckets
    /// Throws if a bucket creation fails
    void addBuckets(size_t bucket_count);

    /// Increase number of buckets to match desired_size.
    /// Called when HashJoin in-memory table for one bucket exceeds the limits.
    ///
    /// NB: after @rehashBuckets there may be rows that are written to the buckets that they do not belong to.
    /// It is fine; these rows will be written to the corresponding buckets during the third stage.
    Buckets rehashBuckets();

    /// Perform some bookkeeping after all calls to @joinBlock.
    void startReadingDelayedBlocks();

    size_t getNumBuckets() const;
    Buckets getCurrentBuckets() const;

    /// Structure block to store in the HashJoin according to sample_block.
    Block prepareRightBlock(const Block & block);

    LoggerPtr log;
    std::shared_ptr<TableJoin> table_join;
    Block left_sample_block;
    Block right_sample_block;
    Block output_sample_block;
    bool any_take_last_row;
    const size_t initial_num_buckets;
    const size_t max_num_buckets;

    Names left_key_names;
    Names right_key_names;

    TemporaryDataOnDiskScopePtr tmp_data;

    Buckets buckets;
    mutable SharedMutex rehash_mutex;

    FileBucket * current_bucket = nullptr;

    mutable std::mutex current_bucket_mutex;

    InMemoryJoinPtr hash_join;
    Block hash_join_sample_block;
    mutable std::mutex hash_join_mutex;
    std::atomic<bool> force_spill = false;
};

}
