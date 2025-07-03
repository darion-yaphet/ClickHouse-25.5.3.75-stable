Contains query planning functionality on top of Query Tree, see src/Analyzer/README.md.

This component is responsible for query planning only if the `enable_analyzer` setting is enabled.
Otherwise, the old planner infrastructure is used. See src/Interpreters/ExpressionAnalyzer.h and src/Interpreters/InterpreterSelectQuery.h.

The `PlannerActionsVisitor` builds `ActionsDAG` from an expression represented as a query tree.
It also creates unique execution names for all nodes in the `ActionsDAG`, i.e., constants.
It is responsible for creating the same execution names for expression on initiator and follower nodes during distributed query execution.

The `PlannerExpressionAnalysis.h` contains the `buildExpressionAnalysisResult` function to calculate information about the stream header after every query step.

`PlannerContext` contains the proper query `Context` and is responsible for creating unique execution names for `ColumnNode`.
`TableExpressionData` for table expression nodes of the corresponding query must be registered in the `PlannerContext`.

Other files contain the implementation of query planning for different parts of the query.

包含在查询树之上实现的查询计划功能，参见 `src/Analyzer/README.md`。

只有在启用 `enable_analyzer` 设置时，此组件才负责查询计划。
否则，将使用旧的计划器基础设施。参见 `src/Interpreters/ExpressionAnalyzer.h` 和 `src/Interpreters/InterpreterSelectQuery.h`。

`PlannerActionsVisitor` 从表示为查询树的表达式中构建 `ActionsDAG`。
它还会为 `ActionsDAG` 中的所有节点（即常量）创建唯一的执行名称。在分布式查询执行期间，它负责为发起节点和跟随节点上的表达式创建相同的执行名称。

`PlannerExpressionAnalysis.h` 包含 `buildExpressionAnalysisResult` 函数，用于计算每个查询步骤之后流头的信息。

`PlannerContext` 包含适当的查询 `Context`，并负责为 `ColumnNode` 创建唯一的执行名称。
必须在 `PlannerContext` 中注册对应查询的表表达式节点的 `TableExpressionData`。

其他文件包含针对查询不同部分的查询计划实现。