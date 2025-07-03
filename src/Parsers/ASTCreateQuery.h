#pragma once

#include <Parsers/ASTQueryWithTableAndOutput.h>
#include <Parsers/ASTQueryWithOnCluster.h>
#include <Parsers/ASTDictionary.h>
#include <Parsers/ASTDictionaryAttributeDeclaration.h>
#include <Parsers/ASTTableOverrides.h>
#include <Parsers/ASTViewTargets.h>
#include <Parsers/ASTSQLSecurity.h>
#include <Parsers/ASTRefreshStrategy.h>
#include <Interpreters/StorageID.h>

namespace DB
{

class ASTFunction;
class ASTSetQuery;
class ASTSelectWithUnionQuery;
struct CreateQueryUUIDs;

/// 存储定义。
class ASTStorage : public IAST
{
public:
    ASTFunction * engine = nullptr;
    IAST * partition_by = nullptr;
    IAST * primary_key = nullptr;
    IAST * order_by = nullptr;
    IAST * sample_by = nullptr;
    IAST * ttl_table = nullptr;
    ASTSetQuery * settings = nullptr;

    String getID(char) const override { return "Storage definition"; }

    ASTPtr clone() const override;

    bool isExtendedStorageDefinition() const;

    void forEachPointerToChild(std::function<void(void**)> f) override
    {
        f(reinterpret_cast<void **>(&engine));
        f(reinterpret_cast<void **>(&partition_by));
        f(reinterpret_cast<void **>(&primary_key));
        f(reinterpret_cast<void **>(&order_by));
        f(reinterpret_cast<void **>(&sample_by));
        f(reinterpret_cast<void **>(&ttl_table));
        f(reinterpret_cast<void **>(&settings));
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & s, FormatState & state, FormatStateStacked frame) const override;
};


/// 表达式列表。
class ASTExpressionList;

/// 列定义。
class ASTColumns : public IAST
{
public:
    ASTExpressionList * columns = nullptr; /// 列定义。
    ASTExpressionList * indices = nullptr; /// 索引定义。
    ASTExpressionList * constraints = nullptr; /// 约束定义。
    ASTExpressionList * projections = nullptr; /// 投影定义。
    IAST              * primary_key = nullptr; /// 主键定义。
    IAST              * primary_key_from_columns = nullptr; /// 从列定义的主键。

    String getID(char) const override { return "Columns definition"; }

    ASTPtr clone() const override;

    /// 检查是否为空。
    bool empty() const
    {
        return (!columns || columns->children.empty()) && (!indices || indices->children.empty()) && (!constraints || constraints->children.empty())
            && (!projections || projections->children.empty());
    }

    /// 遍历子节点。
    void forEachPointerToChild(std::function<void(void**)> f) override
    {
        f(reinterpret_cast<void **>(&columns));
        f(reinterpret_cast<void **>(&indices));
        f(reinterpret_cast<void **>(&primary_key));
        f(reinterpret_cast<void **>(&constraints));
        f(reinterpret_cast<void **>(&projections));
        f(reinterpret_cast<void **>(&primary_key_from_columns));
    }

protected:
    void formatImpl(WriteBuffer & ostr, const FormatSettings & s, FormatState & state, FormatStateStacked frame) const override;
};


/// CREATE TABLE or ATTACH TABLE query
/// 创建表或附加表查询。
class ASTCreateQuery : public ASTQueryWithTableAndOutput, public ASTQueryWithOnCluster
{
public:
    bool attach{false};    /// 查询 ATTACH TABLE, 不是 CREATE TABLE.
    bool if_not_exists{false}; /// 如果表不存在，则创建表。
    bool is_ordinary_view{false}; /// 普通视图。
    bool is_materialized_view{false}; /// 物化视图。
    bool is_live_view{false}; /// 实时视图。
    bool is_window_view{false}; /// 窗口视图。
    bool is_time_series_table{false}; /// CREATE TABLE ... ENGINE=TimeSeries() ...
    bool is_populate{false}; /// 填充表。
    bool is_create_empty{false};    /// CREATE TABLE ... EMPTY AS SELECT ...
    bool is_clone_as{false};    /// CREATE TABLE ... CLONE AS ...
    bool replace_view{false}; /// CREATE OR REPLACE VIEW
    bool has_uuid{false}; // CREATE TABLE x UUID '...'

    ASTColumns * columns_list = nullptr; /// 列定义。
    ASTExpressionList * aliases_list = nullptr; /// 别名，例如 "CREATE VIEW my_view (a, b) AS SELECT 1, 2" 中的 "(a, b)"。
    ASTStorage * storage = nullptr; /// 存储定义。

    ASTPtr watermark_function; /// 水印函数。
    ASTPtr lateness_function; /// 延迟函数。
    String as_database; /// 数据库别名。
    String as_table; /// 表别名。
    IAST * as_table_function = nullptr;
    ASTSelectWithUnionQuery * select = nullptr;
    ASTViewTargets * targets = nullptr;
    IAST * comment = nullptr;
    ASTPtr sql_security = nullptr;

    ASTTableOverrideList * table_overrides = nullptr; /// For CREATE DATABASE with engines that automatically create tables

    bool is_dictionary{false}; /// CREATE DICTIONARY
    ASTExpressionList * dictionary_attributes_list = nullptr; /// attributes of
    ASTDictionary * dictionary = nullptr; /// dictionary definition (layout, primary key, etc.)

    ASTRefreshStrategy * refresh_strategy = nullptr; // For CREATE MATERIALIZED VIEW ... REFRESH ...

    bool is_watermark_strictly_ascending{false}; /// STRICTLY ASCENDING WATERMARK STRATEGY FOR WINDOW VIEW
    bool is_watermark_ascending{false}; /// ASCENDING WATERMARK STRATEGY FOR WINDOW VIEW
    bool is_watermark_bounded{false}; /// BOUNDED OUT OF ORDERNESS WATERMARK STRATEGY FOR WINDOW VIEW
    bool allowed_lateness{false}; /// ALLOWED LATENESS FOR WINDOW VIEW

    bool attach_short_syntax{false};

    std::optional<String> attach_from_path = std::nullopt;

    std::optional<bool> attach_as_replicated = std::nullopt;

    bool replace_table{false};
    bool create_or_replace{false};

    /** Get the text that identifies this element. */
    String getID(char delim) const override;

    ASTPtr clone() const override;

    ASTPtr getRewrittenASTWithoutOnCluster(const WithoutOnClusterASTRewriteParams & params) const override
    {
        return removeOnCluster<ASTCreateQuery>(clone(), params.default_database);
    }

    bool isView() const { return is_ordinary_view || is_materialized_view || is_live_view || is_window_view; }

    bool isParameterizedView() const;

    NameToNameMap getQueryParameters() const;

    bool supportSQLSecurity() const { return is_ordinary_view || is_materialized_view; }

    QueryKind getQueryKind() const override { return QueryKind::Create; }

    /// Generates a random UUID for this create query if it's not specified already.
    /// The function also generates random UUIDs for inner target tables if this create query implies that
    /// (for example, if it's a `CREATE MATERIALIZED VIEW` query with an inner storage).
    /// 为这个创建查询生成一个随机 UUID，如果它还没有指定。
    /// 该函数还为内部目标表生成随机 UUID（如果这个创建查询暗示了这一点，例如，如果它是一个 `CREATE MATERIALIZED VIEW` 查询，并且有内部存储）。
    void generateRandomUUIDs();

    /// Removes UUID from this create query.
    /// The function also removes UUIDs for inner target tables from this create query (see also generateRandomUUID()).
    /// 从这个创建查询中删除 UUID。
    /// 该函数还从这个创建查询中删除内部目标表的 UUID（参见 also generateRandomUUID()）。
    void resetUUIDs();

    /// Returns information about a target table.
    /// If that information isn't specified in this create query (or even not allowed) then the function returns an empty value.
    /// 返回目标表的信息。
    /// 如果这个创建查询中没有指定这个信息（或者甚至不允许），那么函数返回一个空值。
    StorageID getTargetTableID(ViewTarget::Kind target_kind) const;
    /// 检查目标表是否存在。
    bool hasTargetTableID(ViewTarget::Kind target_kind) const;
    /// 返回内部目标表的 UUID。
    UUID getTargetInnerUUID(ViewTarget::Kind target_kind) const;
    /// 检查是否存在内部 UUID。
    bool hasInnerUUIDs() const;
    /// 返回内部目标表的引擎。
    std::shared_ptr<ASTStorage> getTargetInnerEngine(ViewTarget::Kind target_kind) const;
    /// 设置内部目标表的引擎。
    void setTargetInnerEngine(ViewTarget::Kind target_kind, ASTPtr storage_def);

    bool is_materialized_view_with_external_target() const { return is_materialized_view && hasTargetTableID(ViewTarget::To); }
    bool is_materialized_view_with_inner_table() const { return is_materialized_view && !hasTargetTableID(ViewTarget::To); }

protected:
    void formatQueryImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override;

    void forEachPointerToChild(std::function<void(void**)> f) override
    {
        f(reinterpret_cast<void **>(&columns_list));
        f(reinterpret_cast<void **>(&aliases_list));
        f(reinterpret_cast<void **>(&storage));
        f(reinterpret_cast<void **>(&targets));
        f(reinterpret_cast<void **>(&as_table_function));
        f(reinterpret_cast<void **>(&select));
        f(reinterpret_cast<void **>(&comment));
        f(reinterpret_cast<void **>(&table_overrides));
        f(reinterpret_cast<void **>(&dictionary_attributes_list));
        f(reinterpret_cast<void **>(&dictionary));
    }
};

}
