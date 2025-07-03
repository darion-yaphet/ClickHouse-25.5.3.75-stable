#pragma once

#include <Interpreters/IInterpreter.h>
#include <Interpreters/SelectQueryOptions.h>

#include <Processors/QueryPlan/QueryPlan.h>
#include <Storages/SelectQueryInfo.h>

namespace DB
{

class QueryNode;

class GlobalPlannerContext;
using GlobalPlannerContextPtr = std::shared_ptr<GlobalPlannerContext>;

class PlannerContext;
using PlannerContextPtr = std::shared_ptr<PlannerContext>;

class Planner
{
public:
    /// Initialize planner with query tree after analysis phase
    /// 初始化规划器，在分析阶段后使用查询树。
    Planner(const QueryTreeNodePtr & query_tree_,
        SelectQueryOptions & select_query_options_);

    /// Initialize planner with query tree after query analysis phase and global planner context
    /// 初始化规划器，在查询分析阶段和全局规划器上下文后使用查询树。
    Planner(const QueryTreeNodePtr & query_tree_,
        SelectQueryOptions & select_query_options_,
        GlobalPlannerContextPtr global_planner_context_);

    /// Initialize planner with query tree after query analysis phase and planner context
    /// 初始化规划器，在查询分析阶段和规划器上下文后使用查询树。
    Planner(const QueryTreeNodePtr & query_tree_,
        SelectQueryOptions & select_query_options_,
        PlannerContextPtr planner_context_);

    /// 获取查询计划。
    const QueryPlan & getQueryPlan() const
    {
        return query_plan;
    }

    /// 获取查询计划。
    QueryPlan & getQueryPlan()
    {
        return query_plan;
    }

    /// 获取使用的行策略。
    const std::set<std::string> & getUsedRowPolicies() const
    {
        return used_row_policies;
    }

    /// 构建查询计划（如果需要）。
    void buildQueryPlanIfNeeded();

    /// 提取查询计划。
    QueryPlan && extractQueryPlan() &&
    {
        return std::move(query_plan);
    }

    /// 添加存储限制。
    void addStorageLimits(const StorageLimitsList & limits);

    /// 获取规划器上下文。
    PlannerContextPtr getPlannerContext() const
    {
        return planner_context;
    }

    /// We support mapping QueryNode -> QueryPlanStep (the last step added to plan from this query)
    /// It is useful for parallel replicas analysis.
    /// 支持映射 QueryNode -> QueryPlanStep（计划中添加的最后一个步骤）。
    /// 对于并行副本分析很有用。
    using QueryNodeToPlanStepMapping = std::unordered_map<const QueryNode *, const QueryPlan::Node *>;
    const QueryNodeToPlanStepMapping & getQueryNodeToPlanStepMapping() const { return query_node_to_plan_step_mapping; }

private:
    SelectQueryInfo buildSelectQueryInfo() const;

    void buildPlanForUnionNode();

    void buildPlanForQueryNode();

    LoggerPtr log = getLogger("Planner");
    QueryTreeNodePtr query_tree;
    SelectQueryOptions & select_query_options;
    PlannerContextPtr planner_context;
    QueryPlan query_plan;
    StorageLimitsList storage_limits;
    std::set<std::string> used_row_policies;
    QueryNodeToPlanStepMapping query_node_to_plan_step_mapping;
};

}
