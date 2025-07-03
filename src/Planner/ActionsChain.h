#pragma once

#include <Interpreters/ActionsDAG.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

/** Chain of query actions steps. This class is needed to eliminate unnecessary actions calculations.
  * Each step is represented by actions DAG.
  * 查询动作步骤链。这个类需要消除不必要的动作计算。
  * 每个步骤由动作 DAG 表示。
  *
  * Consider such example query:
  * SELECT expr(id) FROM test_table WHERE expr(id) > 0.
  * 考虑这样的查询：
  * SELECT expr(id) FROM test_table WHERE expr(id) > 0.
  *
  * We want to reuse expr(id) from previous expressions step, and not recalculate it in projection.
  * To do this we build a chain of all query action steps.
  * 为了做到这一点，我们构建了所有查询动作步骤的链。 
  *
  * For example:
  * 1. Before where.
  * 2. Before order by.
  * 3. Projection.
  *
  * Initially root of chain is initialized with join tree query plan header.
  * Each next chain step, must be initialized with previous step available output columns.
  * That way we forward all available output columns (functions, columns, aliases) from first step of the chain to the
  * last step. After chain is build we can finalize it.
  * 最初根链初始化为 JOIN 树查询计划头。
  * 每个下一个链步骤必须使用前一个步骤的可用输出列进行初始化。
  * 这样，我们将所有可用输出列（函数、列、别名）从链的第一个步骤传递到最后一个步骤。
  * 在链构建后，我们可以完成它。
  *
  * Each step has input columns (some of them are not necessary) and output columns. Before chain finalize output columns
  * contain only necessary actions for step output calculation.
  * For each step starting from last (i), we add columns that are necessary for this step to previous step (i - 1),
  * and remove unused input columns of previous step(i - 1).
  * That way we reuse already calculated expressions from first step to last step.
  * 每个步骤都有输入列（其中一些是不必要的）和输出列。在链完成前，输出列只包含步骤输出计算所需的必要动作。
  * 从最后一个步骤开始（i），我们将此步骤所需的列添加到前一个步骤（i - 1），并删除前一个步骤（i - 1）的不必要输入列。
  * 这样，我们从第一个步骤到最后一个步骤重用已经计算的表达式。
  */

class ActionsChainStep;
using ActionsChainStepPtr = std::unique_ptr<ActionsChainStep>;
using ActionsChainSteps = std::vector<ActionsChainStepPtr>;

/// Actions chain step represent single step in actions chain.
/// 动作链步骤表示动作链中的单个步骤。
class ActionsChainStep
{
public:
    /** Initialize actions step with actions dag.
      * Input column names initialized using actions dag nodes with INPUT type.
      * If use_actions_nodes_as_output_columns = true output columns are initialized using actions dag nodes.
      * If additional output columns are specified they are added to output columns.
      */
    explicit ActionsChainStep(ActionsAndProjectInputsFlagPtr actions_,
        bool use_actions_nodes_as_output_columns = true,
        ColumnsWithTypeAndName additional_output_columns_ = {});

    /// Get actions
    ActionsAndProjectInputsFlagPtr & getActions()
    {
        return actions;
    }

    /// Get actions
    const ActionsAndProjectInputsFlagPtr & getActions() const
    {
        return actions;
    }

    /// Get available output columns
    const ColumnsWithTypeAndName & getAvailableOutputColumns() const
    {
        return available_output_columns;
    }

    /// Get input column names
    const NameSet & getInputColumnNames() const
    {
        return input_columns_names;
    }

    /** Get child required output columns names.
      * Initialized during finalizeOutputColumns method call.
      */
    const NameSet & getChildRequiredOutputColumnsNames() const
    {
        return child_required_output_columns_names;
    }

    /** Finalize step output columns and remove unnecessary input columns.
      * If actions dag node has same name as child input column, it is added to actions output nodes.
      */
    void finalizeInputAndOutputColumns(const NameSet & child_input_columns);

    /// Dump step into buffer
    void dump(WriteBuffer & buffer) const;

    /// Dump step
    String dump() const;

private:
    void initialize();

    ActionsAndProjectInputsFlagPtr actions;

    bool use_actions_nodes_as_output_columns = true;

    NameSet input_columns_names;

    NameSet child_required_output_columns_names;

    ColumnsWithTypeAndName available_output_columns;

    ColumnsWithTypeAndName additional_output_columns;
};

/// Query actions chain
class ActionsChain
{
public:
    /// Add step into actions chain
    void addStep(ActionsChainStepPtr step)
    {
        steps.emplace_back(std::move(step));
    }

    /// Get steps
    const ActionsChainSteps & getSteps() const
    {
        return steps;
    }

    /// Get steps size
    size_t getStepsSize() const
    {
        return steps.size();
    }

    const ActionsChainStepPtr & at(size_t index) const
    {
        if (index >= steps.size())
            throw std::out_of_range("actions chain access is out of range");

        return steps[index];
    }

    ActionsChainStepPtr & at(size_t index)
    {
        if (index >= steps.size())
            throw std::out_of_range("actions chain access is out of range");

        return steps[index];
    }

    ActionsChainStepPtr & operator[](size_t index)
    {
        return steps[index];
    }

    const ActionsChainStepPtr & operator[](size_t index) const
    {
        return steps[index];
    }

    /// Get last step
    ActionsChainStep * getLastStep()
    {
        return steps.back().get();
    }

    /// Get last step or throw exception if chain is empty
    ActionsChainStep * getLastStepOrThrow()
    {
        if (steps.empty())
            throw Exception(ErrorCodes::LOGICAL_ERROR, "ActionsChain is empty");

        return steps.back().get();
    }

    /// Get last step index
    size_t getLastStepIndex()
    {
        return steps.size() - 1;
    }

    /// Get last step index or throw exception if chain is empty
    size_t getLastStepIndexOrThrow()
    {
        if (steps.empty())
            throw Exception(ErrorCodes::LOGICAL_ERROR, "ActionsChain is empty");

        return steps.size() - 1;
    }

    /// Get last step available output columns
    const ColumnsWithTypeAndName & getLastStepAvailableOutputColumns() const
    {
        return steps.back()->getAvailableOutputColumns();
    }

    /// Get last step available output columns or throw exception if chain is empty
    const ColumnsWithTypeAndName & getLastStepAvailableOutputColumnsOrThrow() const
    {
        if (steps.empty())
            throw Exception(ErrorCodes::LOGICAL_ERROR, "ActionsChain is empty");

        return steps.back()->getAvailableOutputColumns();
    }

    /// Get last step available output columns or null if chain is empty
    const ColumnsWithTypeAndName * getLastStepAvailableOutputColumnsOrNull() const
    {
        if (steps.empty())
            return nullptr;

        return &steps.back()->getAvailableOutputColumns();
    }

    /// Finalize chain
    void finalize();

    /// Dump chain into buffer
    void dump(WriteBuffer & buffer) const;

    /// Dump chain
    String dump() const;

private:
    ActionsChainSteps steps;
};

}
