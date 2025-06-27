#pragma once

#include <Parsers/IAST_fwd.h>
#include <Interpreters/Context_fwd.h>
#include <Processors/Formats/IInputFormat.h>
#include <cstddef>
#include <memory>


namespace DB
{

class Pipe;

/// Prepares a input format, which produce data containing in INSERT query.
/// 准备一个输入格式，用于生成包含在 INSERT 查询中的数据。
InputFormatPtr getInputFormatFromASTInsertQuery(
    const ASTPtr & ast,
    bool with_buffers,
    const Block & header,
    ContextPtr context,
    const ASTPtr & input_function);

/// Prepares a pipe from input format got from ASTInsertQuery,
/// which produce data containing in INSERT query.
/// 准备一个管道，从输入格式中生成包含在 INSERT 查询中的数据。
Pipe getSourceFromInputFormat(
    const ASTPtr & ast,
    InputFormatPtr format,
    ContextPtr context,
    const ASTPtr & input_function);

/// Prepares a pipe which produce data containing in INSERT query.
/// 准备一个管道，从输入格式中生成包含在 INSERT 查询中的数据。
Pipe getSourceFromASTInsertQuery(
    const ASTPtr & ast,
    bool with_buffers,
    const Block & header,
    ContextPtr context,
    const ASTPtr & input_function);

class ReadBuffer;

/// Prepares a read buffer, that allows to read inlined data
/// from ASTInsertQuert directly, and from tail buffer, if it exists.
/// 准备一个读取缓冲区，用于直接从 ASTInsertQuert 中读取数据，以及从尾部缓冲区中读取数据。
std::unique_ptr<ReadBuffer> getReadBufferFromASTInsertQuery(const ASTPtr & ast);

}
