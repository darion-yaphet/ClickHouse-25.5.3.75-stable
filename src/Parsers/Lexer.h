#pragma once

#include <stddef.h>
#include <cstdint>


namespace DB
{

#define APPLY_FOR_TOKENS(M) \
    M(Whitespace) \
    M(Comment) \
    \
    M(BareWord)               /** Either keyword (SELECT) or identifier (column) */ \
    \
    M(Number)                 /** Always non-negative. No leading plus. 123 or something like 123.456e12, 0x123p12 */ \
    M(StringLiteral)          /** 'hello word', 'hello''word', 'hello\'word\\' */ \
    \
    M(QuotedIdentifier)       /** "x", `x` */ \
    \
    M(OpeningRoundBracket) \
    M(ClosingRoundBracket) \
    \
    M(OpeningSquareBracket) \
    M(ClosingSquareBracket) \
    \
    M(OpeningCurlyBrace) \
    M(ClosingCurlyBrace) \
    \
    M(Comma) \
    M(Semicolon) \
    M(VerticalDelimiter)      /** Vertical delimiter \G */ \
    M(Dot)                    /** Compound identifiers, like a.b or tuple access operator a.1, (x, y).2. */ \
                              /** Need to be distinguished from floating point number with omitted integer part: .1 */ \
    \
    M(Asterisk)               /** Could be used as multiplication operator or on it's own: "SELECT *" */ \
    \
    M(HereDoc) \
    \
    M(DollarSign) \
    M(Plus) \
    M(Minus) \
    M(Slash) \
    M(Percent) \
    M(Arrow)                  /** ->. Should be distinguished from minus operator. */ \
    M(QuestionMark) \
    M(Colon) \
    M(Caret) \
    M(DoubleColon) \
    M(Equals) \
    M(NotEquals) \
    M(Less) \
    M(Greater) \
    M(LessOrEquals) \
    M(GreaterOrEquals) \
    M(Spaceship)              /** <=>. Used in MySQL for NULL-safe equality comparison. */ \
    M(PipeMark) \
    M(Concatenation)          /** String concatenation operator: || */ \
    \
    M(At)                     /** @. Used for specifying user names and also for MySQL-style variables. */ \
    M(DoubleAt)               /** @@. Used for MySQL-style global variables. */ \
    \
    /** Order is important. EndOfStream goes after all usual tokens, and special error tokens goes after EndOfStream. */ \
    \
    M(EndOfStream) \
    \
    /** Something unrecognized. */ \
    M(Error) \
    /** Something is wrong and we have more information. */ \
    M(ErrorMultilineCommentIsNotClosed) \
    M(ErrorSingleQuoteIsNotClosed) \
    M(ErrorDoubleQuoteIsNotClosed) \
    M(ErrorBackQuoteIsNotClosed) \
    M(ErrorSingleExclamationMark) \
    M(ErrorSinglePipeMark) \
    M(ErrorWrongNumber) \
    M(ErrorMaxQuerySizeExceeded) \


enum class TokenType : uint8_t
{
#define M(TOKEN) TOKEN,
APPLY_FOR_TOKENS(M)
#undef M
};

const char * getTokenName(TokenType type);
const char * getErrorTokenDescription(TokenType type);


struct Token
{
    TokenType type;
    const char * begin;
    const char * end;

    size_t size() const { return end - begin; }

    Token() = default;
    Token(TokenType type_, const char * begin_, const char * end_) : type(type_), begin(begin_), end(end_) {}

    bool isSignificant() const { return type != TokenType::Whitespace && type != TokenType::Comment; }
    bool isError() const { return type > TokenType::EndOfStream; }
    bool isEnd() const { return type == TokenType::EndOfStream; }
};

/// 词法分析器。
class Lexer
{
public:
    /// 构造函数。
    Lexer(const char * begin_, const char * end_, size_t max_query_size_ = 0)
            : begin(begin_), pos(begin_), end(end_), max_query_size(max_query_size_) {}

    /// 获取下一个词法单元。
    Token nextToken();

private:
    const char * const begin;
    const char * pos;
    const char * const end;

    const size_t max_query_size;

    Token nextTokenImpl();

    /// This is needed to disambiguate tuple access operator from floating point number (.1).
    /// 需要区分元组访问操作符和浮点数（.1）。
    TokenType prev_significant_token_type = TokenType::Whitespace;   /// No previous token.
};

}
