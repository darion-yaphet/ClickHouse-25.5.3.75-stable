#pragma once

#include <QueryPipeline/BlockIO.h>
#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>
#include <Storages//IStorage_fwd.h>

namespace DB
{

struct QueryLogElement;

/** Interpreters interface for different queries.
  * 解释器接口，用于不同的查询。
  */
class IInterpreter
{
public:
    /** For queries that return a result (SELECT and similar), sets in BlockIO a stream from which you can read this result.
      * 对于返回结果的查询（SELECT 和类似查询），在 BlockIO 中设置一个流，可以从该流中读取结果。
      *
      * For queries that receive data (INSERT), sets a thread in BlockIO where you can write data.
      * 对于接收数据的查询（INSERT），在 BlockIO 中设置一个线程，可以在该线程中写入数据。
      *
      * For queries that do not require data and return nothing, BlockIO will be empty.
      * 对于不需要数据且不返回任何内容的查询，BlockIO 将为空。
      */
    virtual BlockIO execute() = 0;

    virtual bool ignoreQuota() const { return false; }
    virtual bool ignoreLimits() const { return false; }

    // Fill query log element with query kind, query databases, query tables and query columns.
    // 填充查询日志元素，包括查询类型、查询数据库、查询表和查询列。
    void extendQueryLogElem(
        QueryLogElement & elem,
        const ASTPtr & ast,
        ContextPtr context,
        const String & query_database,
        const String & query_table) const;

    virtual void extendQueryLogElemImpl(QueryLogElement &, const ASTPtr &, ContextPtr) const {}

    /// Returns true if transactions maybe supported for this type of query.
    // 返回 true 表示该类型的查询可能支持事务。
    /// If Interpreter returns true, than it is responsible to check that specific query with specific Storage is supported.
    // 如果解释器返回 true，则它负责检查特定查询与特定存储是否支持事务。
    virtual bool supportsTransactions() const { return false; }

    /// Helper function for some Interpreters.
    // 检查存储是否支持事务。
    static void checkStorageSupportsTransactionsIfNeeded(const StoragePtr & storage, ContextPtr context, bool is_readonly_query = false);

    virtual ~IInterpreter() = default;
};

}
