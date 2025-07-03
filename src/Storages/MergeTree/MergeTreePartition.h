#pragma once

#include <base/types.h>
#include <IO/WriteBufferFromFileBase.h>
#include <Storages/KeyDescription.h>
#include <Core/Field.h>

namespace DB
{

class Block;
class MergeTreeData;
struct FormatSettings;
struct MergeTreeDataPartChecksums;
struct StorageInMemoryMetadata;
class IDataPartStorage;
class IMergeTreeDataPart;
struct WriteSettings;

using StorageMetadataPtr = std::shared_ptr<const StorageInMemoryMetadata>;
using MutableDataPartStoragePtr = std::shared_ptr<IDataPartStorage>;

/// This class represents a partition value of a single part and encapsulates its loading/storing logic.
/// 合并树分区
struct MergeTreePartition
{
    Row value;

    MergeTreePartition() = default;

    explicit MergeTreePartition(Row value_) : value(std::move(value_)) {}

    /// For month-based partitioning.
    explicit MergeTreePartition(UInt32 yyyymm) : value(1, yyyymm) {}

    // 获取分区 ID
    String getID(const MergeTreeData & storage) const;
    String getID(const Block & partition_key_sample) const;

    // 尝试从分区 ID 解析值
    static std::optional<Row> tryParseValueFromID(const String & partition_id, const Block & partition_key_sample);

    // 序列化文本
    void serializeText(StorageMetadataPtr metadata_snapshot, WriteBuffer & out, const FormatSettings & format_settings) const;
    String serializeToString(StorageMetadataPtr metadata_snapshot) const;

    // 加载
    void load(const IMergeTreeDataPart & part);

    /// Store functions return write buffer with written but not finalized data.
    /// User must call finish() for returned object.
    /// 存储函数返回写入缓冲区，其中写入了但未最终确定的数据。
    /// 用户必须调用 finish() 返回的对象。
    [[nodiscard]] std::unique_ptr<WriteBufferFromFileBase> store(
        StorageMetadataPtr metadata_snapshot,
        ContextPtr storage_context,
        IDataPartStorage & data_part_storage,
        MergeTreeDataPartChecksums & checksums) const;

    // 存储函数返回写入缓冲区，其中写入了但未最终确定的数据。
    // 用户必须调用 finish() 返回的对象。
    [[nodiscard]] std::unique_ptr<WriteBufferFromFileBase> store(
        const Block & partition_key_sample,
        IDataPartStorage & data_part_storage,
        MergeTreeDataPartChecksums & checksums,
        const WriteSettings & settings) const;

    // 赋值
    void assign(const MergeTreePartition & other) { value = other.value; }

    // 创建
    void create(const StorageMetadataPtr & metadata_snapshot, Block block, size_t row, ContextPtr context);

    /// Adjust partition key and execute its expression on block. Return sample block according to used expression.
    // 调整分区键并执行其表达式。返回根据使用的表达式生成的样本块。
    static NamesAndTypesList executePartitionByExpression(const StorageMetadataPtr & metadata_snapshot, Block & block, ContextPtr context);

    /// Make a modified partition key with substitution from modulo to moduloLegacy. Used in paritionPruner.
    // 使用模数替换模数。用于分区修剪器。
    static KeyDescription adjustPartitionKey(const StorageMetadataPtr & metadata_snapshot, ContextPtr context);
};

}
