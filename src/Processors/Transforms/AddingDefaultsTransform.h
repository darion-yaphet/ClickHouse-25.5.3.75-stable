#pragma once

#include <Processors/ISimpleTransform.h>
#include <Storages/ColumnsDescription.h>


namespace DB
{

class IInputFormat;

/// Adds defaults to columns using BlockDelayedDefaults bitmask attached to Block by child InputStream.
/// 使用BlockDelayedDefaults位掩码附加到Block的子InputStream上，添加默认值到列中。
class AddingDefaultsTransform : public ISimpleTransform
{
public:
    AddingDefaultsTransform(
        const Block & header,
        const ColumnsDescription & columns_,
        IInputFormat & input_format_,
        ContextPtr context_);

    String getName() const override { return "AddingDefaultsTransform"; }

protected:
    void transform(Chunk & chunk) override;

private:
    const ColumnsDescription columns;
    const ColumnDefaults column_defaults;
    IInputFormat & input_format;
    ContextPtr context;
};

}
