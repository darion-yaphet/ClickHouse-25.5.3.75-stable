#pragma once
#include <Processors/QueryPlan/IQueryPlanStep.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int NOT_IMPLEMENTED;
}

/// Step which has single input and single output data stream.
/// 具有单个输入和单个输出数据流的步骤。
/// It doesn't mean that pipeline has single port before or after such step.
/// 它不意味着管道在步骤之前或之后有单个端口。
class ITransformingStep : public IQueryPlanStep
{
public:
    /// This flags are used to automatically set properties for output stream.
    /// 这些标志用于自动设置输出流属性。
    /// They are specified in constructor and cannot be changed.
    /// 它们在构造函数中指定，不能更改。
    struct DataStreamTraits
    {
        /// True if pipeline has single output port after this step.
        /// 如果管道在步骤之后有单个输出端口，则为真。
        /// Examples: MergeSortingStep, AggregatingStep
        bool returns_single_stream;

        /// Won't change the number of ports for pipeline.
        /// 不会改变管道的端口数量。
        /// Examples: true for ExpressionStep, false for MergeSortingStep
        bool preserves_number_of_streams;

        /// Doesn't change row order.
        /// 不会改变行顺序。
        /// Examples: true for FilterStep, false for PartialSortingStep
        bool preserves_sorting;
    };

    /// This flags are used by QueryPlan optimizers.
    /// 这些标志用于QueryPlan优化器。
    /// They can be changed after some optimizations.
    /// 它们可以在某些优化后更改。
    struct TransformTraits
    {
        /// Won't change the total number of rows.
        /// 不会改变总行数。
        /// Examples: true for ExpressionStep (without join or array join), false for FilterStep
        bool preserves_number_of_rows;
    };

    /// 这些标志用于描述步骤的特性。
    struct Traits
    {
        DataStreamTraits data_stream_traits;
        TransformTraits transform_traits;
    };

    ITransformingStep(Header input_header, Header output_header, Traits traits, bool collect_processors_ = true);
    ITransformingStep(const ITransformingStep &) = default;

    QueryPipelineBuilderPtr updatePipeline(QueryPipelineBuilders pipelines, const BuildQueryPipelineSettings & settings) override;

    /// Append processors from the current step to the query pipeline.
    /// 将当前步骤的处理器添加到查询管道中。
    /// Step always has a single input stream, so we implement updatePipeline over this function.
    /// 步骤总是有一个输入流，所以我们通过这个函数实现 updatePipeline。
    virtual void transformPipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings & settings) = 0;

    const TransformTraits & getTransformTraits() const { return transform_traits; }
    const DataStreamTraits & getDataStreamTraits() const { return data_stream_traits; }

    void describePipeline(FormatSettings & settings) const override;

    /// Enforcement is supposed to be done through the special settings that will be taken into account by remote nodes during query planning (e.g. force_aggregation_in_order).
    /// 强制执行应该通过特殊的设置来完成，这些设置将在查询计划期间由远程节点考虑（例如 force_aggregation_in_order）。
    /// Should be called only if data_stream_traits.can_enforce_sorting_properties_in_distributed_query == true.
    /// 应该只在 data_stream_traits.can_enforce_sorting_properties_in_distributed_query == true 时调用。
    virtual void adjustSettingsToEnforceSortingPropertiesInDistributedQuery(ContextMutablePtr) const
    {
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Not implemented");
    }

protected:
    TransformTraits transform_traits;

private:
    /// If we should collect processors got after pipeline transformation.
    bool collect_processors;

    const DataStreamTraits data_stream_traits;
};

}
