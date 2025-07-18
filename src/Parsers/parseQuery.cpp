#include <Parsers/parseQuery.h>

#include <Interpreters/OpenTelemetrySpanLog.h>
#include <Parsers/ParserQuery.h>
#include <Parsers/ASTInsertQuery.h>
#include <Parsers/ASTExplainQuery.h>
#include <Parsers/CommonParsers.h>
#include <Parsers/Lexer.h>
#include <Parsers/TokenIterator.h>
#include <Common/StringUtils.h>
#include <Common/typeid_cast.h>
#include <Common/UTF8Helpers.h>
#include <base/find_symbols.h>
#include <IO/WriteHelpers.h>
#include <IO/WriteBufferFromString.h>
#include <IO/Operators.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int SYNTAX_ERROR;
}

namespace
{

/** From position in (possible multiline) query, get line number and column number in line.
  * Used in syntax error message.
  * 从位置（可能多行）查询中获取行号和列号。
  * 用于语法错误消息。
  */
std::pair<size_t, size_t> getLineAndCol(const char * begin, const char * pos)
{
    size_t line = 0;

    const char * nl;
    while ((nl = find_first_symbols<'\n'>(begin, pos)) < pos)
    {
        ++line;
        begin = nl + 1;
    }

    /// Lines numbered from 1.
    return { line + 1, pos - begin + 1 };
}

/// 写入预期
WriteBuffer & operator<< (WriteBuffer & out, const Expected & expected)
{
    if (expected.variants.empty())
        return out;

    if (expected.variants.size() == 1)
        return out << *expected.variants.begin();

    out << "one of: ";
    bool first = true;
    for (const auto & variant : expected.variants)
    {
        if (!first)
            out << ", ";
        first = false;

        out << variant;
    }
    return out;
}


/// Highlight the place of syntax error.
/// 突出显示语法错误的位置。
void writeQueryWithHighlightedErrorPositions(
    WriteBuffer & out,
    const char * begin,
    const char * end,
    const Token * positions_to_hilite,   /// must go in ascending order
    size_t num_positions_to_hilite)
{
    const char * pos = begin;
    for (size_t position_to_hilite_idx = 0; position_to_hilite_idx < num_positions_to_hilite; ++position_to_hilite_idx)
    {
        const char * current_position_to_hilite = positions_to_hilite[position_to_hilite_idx].begin;

        assert(current_position_to_hilite <= end);
        assert(current_position_to_hilite >= begin);

        out.write(pos, current_position_to_hilite - pos);

        if (current_position_to_hilite == end)
        {
            out << "\033[41;1m \033[0m";
            return;
        }

        ssize_t bytes_to_hilite = std::min<ssize_t>(UTF8::seqLength(*current_position_to_hilite), end - current_position_to_hilite);

        /// Bright on red background.
        out << "\033[41;1m";
        out.write(current_position_to_hilite, bytes_to_hilite);
        out << "\033[0m";
        pos = current_position_to_hilite + bytes_to_hilite;
    }
    out.write(pos, end - pos);
}

/// 写入查询周围错误。
void writeQueryAroundTheError(
    WriteBuffer & out,
    const char * begin,
    const char * end,
    bool hilite,
    const Token * positions_to_hilite,
    size_t num_positions_to_hilite)
{
    if (hilite)
    {
        out << ":\n\n";
        writeQueryWithHighlightedErrorPositions(out, begin, end, positions_to_hilite, num_positions_to_hilite);
        out << "\n\n";
    }
    else
    {
        if (num_positions_to_hilite)
        {
            const char * example_begin = positions_to_hilite[0].begin;
            size_t total_bytes = end - example_begin;
            size_t show_bytes = UTF8::computeBytesBeforeWidth(
                reinterpret_cast<const UInt8 *>(example_begin), total_bytes, 0, SHOW_CHARS_ON_SYNTAX_ERROR);
            out << ": " << std::string(example_begin, show_bytes) << (show_bytes < total_bytes ? "... " : ". ");
        }
    }
}

/// 写入通用错误消息。
void writeCommonErrorMessage(
    WriteBuffer & out,
    const char * begin,
    const char * end,
    Token last_token,
    const std::string & query_description)
{
    out << "Syntax error";

    if (!query_description.empty())
        out << " (" << query_description << ")";

    out << ": failed at position " << (last_token.begin - begin + 1);

    if (last_token.type == TokenType::EndOfStream || last_token.type == TokenType::Semicolon)
    {
        out << " (end of query)";
    }
    else
    {
        /// Do not print too long tokens.
        size_t token_size_bytes = last_token.end - last_token.begin;
        size_t token_preview_size_bytes = UTF8::computeBytesBeforeWidth(
            reinterpret_cast<const UInt8 *>(last_token.begin), token_size_bytes, 0, SHOW_CHARS_ON_SYNTAX_ERROR);

        out << " (" << std::string(last_token.begin, token_preview_size_bytes)
            << (token_preview_size_bytes < token_size_bytes ? "..." : "") << ")";
    }

    /// If query is multiline.
    const char * nl = find_first_symbols<'\n'>(begin, end);
    if (nl + 1 < end)
    {
        size_t line = 0;
        size_t col = 0;
        std::tie(line, col) = getLineAndCol(begin, last_token.begin);

        out << " (line " << line << ", col " << col << ")";
    }
}

/// 获取语法错误消息。
std::string getSyntaxErrorMessage(
    const char * begin,
    const char * end,
    Token last_token,
    const Expected & expected,
    bool hilite,
    const std::string & query_description)
{
    WriteBufferFromOwnString out;
    writeCommonErrorMessage(out, begin, end, last_token, query_description);
    writeQueryAroundTheError(out, begin, end, hilite, &last_token, 1);

    if (!expected.variants.empty())
        out << "Expected " << expected;

    return out.str();
}

/// 获取词法错误消息。
std::string getLexicalErrorMessage(
    const char * begin,
    const char * end,
    Token last_token,
    bool hilite,
    const std::string & query_description)
{
    WriteBufferFromOwnString out;
    out << getErrorTokenDescription(last_token.type) << ": ";
    writeCommonErrorMessage(out, begin, end, last_token, query_description);
    writeQueryAroundTheError(out, begin, end, hilite, &last_token, 1);
    return out.str();
}

/// 获取未匹配括号错误消息。
std::string getUnmatchedParenthesesErrorMessage(
    const char * begin,
    const char * end,
    const UnmatchedParentheses & unmatched_parens,
    bool hilite,
    const std::string & query_description)
{
    WriteBufferFromOwnString out;
    writeCommonErrorMessage(out, begin, end, unmatched_parens[0], query_description);
    writeQueryAroundTheError(out, begin, end, hilite, unmatched_parens.data(), unmatched_parens.size());

    out << "Unmatched parentheses: ";
    for (const Token & paren : unmatched_parens)
        out << *paren.begin;

    return out.str();
}

}

/// 获取 INSERT 查询的 AST。
static ASTInsertQuery * getInsertAST(const ASTPtr & ast)
{
    /// Either it is INSERT or EXPLAIN INSERT.
    /// 要么是 INSERT 查询，要么是 EXPLAIN INSERT 查询。
    if (auto * explain = ast->as<ASTExplainQuery>())
    {
        if (auto explained_query = explain->getExplainedQuery())
        {
            return explained_query->as<ASTInsertQuery>();
        }
    }
    else
    {
        return ast->as<ASTInsertQuery>();
    }

    return nullptr;
}

/// 获取 INSERT 查询的数据。
const char * getInsertData(const ASTPtr & ast)
{
    if (const ASTInsertQuery * insert = getInsertAST(ast))
        return insert->data;
    return nullptr;
}

/// 尝试解析查询。
ASTPtr tryParseQuery(
    IParser & parser,
    const char * & _out_query_end, /* also query begin as input parameter */
    const char * all_queries_end,
    std::string & out_error_message,
    bool hilite,
    const std::string & query_description,
    bool allow_multi_statements,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks,
    bool skip_insignificant)
{
    /// 获取查询开始位置
    const char * query_begin = _out_query_end;
    Tokens tokens(query_begin, all_queries_end, max_query_size, skip_insignificant);

    /// NOTE: consider use UInt32 for max_parser_depth setting.
    /// NOTE: 考虑使用 UInt32 设置 max_parser_depth。
    IParser::Pos token_iterator(tokens, static_cast<uint32_t>(max_parser_depth), static_cast<uint32_t>(max_parser_backtracks));

    /// 如果查询结束或以分号结束，则返回空
    if (token_iterator->isEnd()
        || token_iterator->type == TokenType::Semicolon)
    {
        out_error_message = "Empty query";
        // Token iterator skips over comments, so we'll get this error for queries
        // like this:
        // "
        // -- just a comment
        // ;
        //"
        // Advance the position, so that we can use this parser for stream parsing
        // even in presence of such queries.
        _out_query_end = token_iterator->begin;
        return nullptr;
    }

    Expected expected;

    /** A shortcut - if Lexer found invalid tokens, fail early without full parsing.
      * But there are certain cases when invalid tokens are permitted:
      * 
      * 一个快捷方式 - 如果Lexer发现无效的tokens，在完全解析之前失败。
      * 但是有一些情况是允许无效的tokens的：
      *
      * 1. INSERT queries can have arbitrary data after the FORMAT clause, that is parsed by a different parser.
      * 1. INSERT查询可以有任意数据，这些数据由不同的解析器解析。
      *
      * 2. It can also be the case when there are multiple queries separated by semicolons, and the first queries are ok
      * while subsequent queries have syntax errors.
      * 2. 也可以是多个查询用分号隔开，前面的查询是正确的，后面的查询有语法错误。
      *
      * This shortcut is needed to avoid complex backtracking in case of obviously erroneous queries.
      * 这个快捷方式是必要的，以避免在明显错误的查询情况下复杂的回溯。
      */
    /// 创建一个lookahead迭代器
    IParser::Pos lookahead(token_iterator);
    if (!ParserKeyword(Keyword::INSERT_INTO).ignore(lookahead))
    {
        while (lookahead->type != TokenType::Semicolon && lookahead->type != TokenType::EndOfStream)
        {
            if (lookahead->isError())
            {
                out_error_message = getLexicalErrorMessage(query_begin, all_queries_end, *lookahead, hilite, query_description);
                return nullptr;
            }

            ++lookahead;
        }

        /// We should not spoil the info about maximum parsed position in the original iterator.
        tokens.reset();
    }

    ASTPtr res;
    const bool parse_res = parser.parse(token_iterator, res, expected);
    const auto last_token = token_iterator.max();
    _out_query_end = last_token.end;

    /// Also check on the AST level, because the generated AST depth can be greater than the recursion depth of the parser.
    if (res && max_parser_depth)
        res->checkDepth(max_parser_depth);

    /// If parsed query ends at data for insertion. Data for insertion could be
    /// in any format and not necessary be lexical correct, so we can't perform
    /// most of the checks.
    if (res && getInsertData(res))
        return res;

    // More granular checks for queries other than INSERT w/inline data.
    // 对除了 INSERT 查询之外的查询进行更细粒度的检查。
    /// Lexical error
    if (last_token.isError())
    {
        out_error_message = getLexicalErrorMessage(query_begin, all_queries_end,
            last_token, hilite, query_description);
        return nullptr;
    }

    /// Unmatched parentheses
    /// 未匹配的括号
    UnmatchedParentheses unmatched_parens = checkUnmatchedParentheses(TokenIterator(tokens));
    if (!unmatched_parens.empty())
    {
        out_error_message = getUnmatchedParenthesesErrorMessage(query_begin,
            all_queries_end, unmatched_parens, hilite, query_description);
        return nullptr;
    }

    if (!parse_res)
    {
        /// Generic parse error.
        out_error_message = getSyntaxErrorMessage(query_begin, all_queries_end,
            last_token, expected, hilite, query_description);
        return nullptr;
    }

    /// Excessive input after query. Parsed query must end with end of data or semicolon or data for INSERT.
    /// 查询后有过多输入。解析的查询必须以结束数据或分号或 INSERT 的数据结束。
    if (!token_iterator->isEnd()
        && token_iterator->type != TokenType::Semicolon)
    {
        expected.add(last_token.begin, "end of query");
        out_error_message = getSyntaxErrorMessage(query_begin, all_queries_end,
            last_token, expected, hilite, query_description);
        return nullptr;
    }

    // Skip the semicolon that might be left after parsing the VALUES format.
    /// 跳过可能留下的分号，在解析 VALUES 格式后。
    while (token_iterator->type == TokenType::Semicolon)
    {
        ++token_iterator;
    }

    // If multi-statements are not allowed, then after semicolon, there must
    // be no non-space characters.
    /// 如果多语句不允许多语句，则分号后必须没有非空字符。
    if (!allow_multi_statements
        && !token_iterator->isEnd())
    {
        out_error_message = getSyntaxErrorMessage(query_begin, all_queries_end,
            last_token, {}, hilite,
            (query_description.empty() ? std::string() : std::string(". "))
                + "Multi-statements are not allowed");
        return nullptr;
    }

    return res;
}

/// 解析查询并移动位置
ASTPtr parseQueryAndMovePosition(
    IParser & parser,
    const char * & pos,
    const char * end,
    const std::string & query_description,
    bool allow_multi_statements,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks)
{
    std::string error_message;
    ASTPtr res = tryParseQuery(
        parser, pos, end, error_message, false, query_description, allow_multi_statements,
        max_query_size, max_parser_depth, max_parser_backtracks, true);

    if (res)
        return res;

    throw Exception::createDeprecated(error_message, ErrorCodes::SYNTAX_ERROR);
}


ASTPtr parseQuery(
    IParser & parser,
    const char * begin,
    const char * end,
    const std::string & query_description,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks)
{
    return parseQueryAndMovePosition(parser, begin, end, query_description, false, max_query_size, max_parser_depth, max_parser_backtracks);
}


ASTPtr parseQuery(
    IParser & parser,
    const std::string & query,
    const std::string & query_description,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks)
{
    return parseQuery(parser, query.data(), query.data() + query.size(), query_description, max_query_size, max_parser_depth, max_parser_backtracks);
}


ASTPtr parseQuery(
    IParser & parser,
    const std::string & query,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks)
{
    return parseQuery(parser, query.data(), query.data() + query.size(), parser.getName(), max_query_size, max_parser_depth, max_parser_backtracks);
}


/// 分割多语句查询。
std::pair<const char *, bool> splitMultipartQuery(
    const std::string & queries,
    std::vector<std::string> & queries_list,
    size_t max_query_size,
    size_t max_parser_depth,
    size_t max_parser_backtracks,
    bool allow_settings_after_format_in_insert,
    bool implicit_select)
{
    ASTPtr ast;

    const char * begin = queries.data(); /// begin of current query
    const char * pos = begin; /// parser moves pos from begin to the end of current query
    const char * end = begin + queries.size();

    ParserQuery parser(end, allow_settings_after_format_in_insert, implicit_select);

    queries_list.clear();

    while (pos < end)
    {
        begin = pos;

        ast = parseQueryAndMovePosition(parser, pos, end, "", true, max_query_size, max_parser_depth, max_parser_backtracks);

        if (ASTInsertQuery * insert = getInsertAST(ast); insert && insert->data)
        {
            /// Data for INSERT is broken on the new line
            pos = insert->data;
            while (*pos && *pos != '\n')
                ++pos;
            insert->end = pos;
        }

        queries_list.emplace_back(queries.substr(begin - queries.data(), pos - begin));

        while (isWhitespaceASCII(*pos) || *pos == ';')
            ++pos;
    }

    return std::make_pair(begin, pos == end);
}


}
