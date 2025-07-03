#pragma once

#include <absl/container/inlined_vector.h>
#include <set>
#include <algorithm>
#include <memory>

#include <Core/Defines.h>
#include <Parsers/IAST_fwd.h>
#include <Parsers/TokenIterator.h>
#include <base/types.h>
#include <Common/Exception.h>
#include <Common/checkStackSize.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int TOO_DEEP_RECURSION;
    extern const int LOGICAL_ERROR;
}

enum class Highlight : uint8_t
{
    none = 0,
    keyword,
    identifier,
    function,
    alias,
    substitution,
    number,
    string,
};

struct HighlightedRange
{
    const char * begin;
    const char * end;
    Highlight highlight;

    auto operator<=>(const HighlightedRange & other) const
    {
        return begin <=> other.begin;
    }
};


/** Collects variants, how parser could proceed further at rightmost position.
  * Also collects a mapping of parsed ranges for highlighting,
  * which is accumulated through the parsing.
  * 收集解析器可以进一步前进的变体，
  * 还收集解析的范围内的高亮映射，
  * 通过解析累积。
  */
struct Expected
{
    absl::InlinedVector<const char *, 7> variants;
    const char * max_parsed_pos = nullptr;

    bool enable_highlighting = false;
    std::set<HighlightedRange> highlights;

    /// 'description' should be statically allocated string.
    ALWAYS_INLINE void add(const char * current_pos, const char * description)
    {
        if (!max_parsed_pos || current_pos > max_parsed_pos)
        {
            variants.clear();
            max_parsed_pos = current_pos;
            variants.push_back(description);
            return;
        }

        if ((current_pos == max_parsed_pos) && (std::find(variants.begin(), variants.end(), description) == variants.end()))
            variants.push_back(description);
    }

    ALWAYS_INLINE void add(TokenIterator it, const char * description)
    {
        add(it->begin, description);
    }

    void highlight(HighlightedRange range);
};


/** Interface for parser classes
  * 解析器接口
  */
class IParser
{
public:
    /// Token iterator augmented with depth information. This allows to control recursion depth.
    /// Token迭代器，增加了深度信息。这允许控制递归深度。
    struct Pos : TokenIterator
    {
        uint32_t depth = 0;
        uint32_t max_depth = 0;

        uint32_t backtracks = 0;
        uint32_t max_backtracks = 0;

        Pos(Tokens & tokens_, uint32_t max_depth_, uint32_t max_backtracks_)
            : TokenIterator(tokens_), max_depth(max_depth_), max_backtracks(max_backtracks_)
        {
        }

        Pos(TokenIterator token_iterator_, uint32_t max_depth_, uint32_t max_backtracks_)
            : TokenIterator(token_iterator_), max_depth(max_depth_), max_backtracks(max_backtracks_)
        {
        }

        /// 增加深度
        ALWAYS_INLINE void increaseDepth()
        {
            ++depth;
            if (unlikely(max_depth > 0 && depth > max_depth))
                throw Exception(ErrorCodes::TOO_DEEP_RECURSION, "Maximum parse depth ({}) exceeded. "
                    "Consider rising max_parser_depth parameter.", max_depth);

            /** Sometimes the maximum parser depth can be set to a high value by the user,
              * but we still want to avoid stack overflow.
              * For this purpose, we can use the checkStackSize function, but it is too heavy.
              * The solution is to check not too frequently.
              * The frequency is arbitrary, but not too large, not too small,
              * and a power of two to simplify the division.
              * 翻译：
              * 有时最大解析深度可以由用户设置为高值，
              * 但我们仍然希望避免堆栈溢出。
              * 为此，我们可以使用checkStackSize函数，但它太重了。
              * 解决方案是不要过于频繁地检查。
              * 频率是任意的，但不要太小，不要太频繁，
              * 并且是2的幂，以简化除法。
              */
#if defined(USE_MUSL) || defined(SANITIZER) || !defined(NDEBUG)
            static constexpr uint32_t check_frequency = 128;
#else
            static constexpr uint32_t check_frequency = 8192;
#endif
            if (depth % check_frequency == 0)
                checkStackSize();
        }

        /// 减少深度
        ALWAYS_INLINE void decreaseDepth()
        {
            if (unlikely(depth == 0))
                throw Exception(ErrorCodes::LOGICAL_ERROR, "Logical error in parser: incorrect calculation of parse depth");
            --depth;
        }

        Pos(const Pos & rhs) = default;

        Pos & operator=(const Pos & rhs);
    };

    /** Get the text of this parser parses. */
    /// 获取解析器的名称
    virtual const char * getName() const = 0;

    /** Parse piece of text from position `pos`, but not beyond end of line (`end` - position after end of line),
      * move pointer `pos` to the maximum position to which it was possible to parse,
      * in case of success return `true` and the result in `node` if it is needed, otherwise false,
      * in `expected` write what was expected in the maximum position,
      *  to which it was possible to parse if parsing was unsuccessful,
      *  or what this parser parse if parsing was successful.
      * The string to which the [begin, end) range is included may be not 0-terminated.
      * 
      * 从位置`pos`解析文本，但不超过行尾(`end` - 行尾后的位置)，
      * 移动指针`pos`到可以解析的最大位置，
      * 如果成功，返回`true`并返回结果`node`，否则返回false，
      * 在`expected`中写入可以解析的最大位置，
      * 如果解析失败，则写入可以解析的最大位置，
      * 如果解析成功，则写入解析器解析的内容。
      */
    virtual bool parse(Pos & pos, ASTPtr & node, Expected & expected) = 0;

    bool ignore(Pos & pos, Expected & expected)
    {
        ASTPtr ignore_node;
        return parse(pos, ignore_node, expected);
    }

    bool ignore(Pos & pos)
    {
        Expected expected;
        return ignore(pos, expected);
    }

    /** The same, but do not move the position and do not write the result to node.
      * 与上面相同，但不会移动位置，也不会将结果写入节点。
      */
    bool check(Pos & pos, Expected & expected)
    {
        Pos begin = pos;
        ASTPtr node;
        if (!parse(pos, node, expected))
        {
            pos = begin;
            return false;
        }
        return true;
    }

    /** The same, but doesn't move the position even if parsing was successful.
      * 与上面相同，但即使解析成功也不会移动位置。
     */
    bool checkWithoutMoving(Pos pos, Expected & expected)
    {
        ASTPtr node;
        return parse(pos, node, expected);
    }

    /** If the parsed fragment should be highlighted in the query editor,
      * which type of highlighting to use?
      * 如果解析的片段应该在查询编辑器中高亮，
      * 使用哪种高亮类型？
      */
    virtual Highlight highlight() const
    {
        return Highlight::none;
    }

    virtual ~IParser() = default;
};

using ParserPtr = std::unique_ptr<IParser>;

}
