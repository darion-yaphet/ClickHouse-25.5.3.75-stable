#include <Parsers/IAST.h>

#include <IO/Operators.h>
#include <IO/WriteBufferFromString.h>
#include <IO/WriteHelpers.h>
#include <Parsers/CommonParsers.h>
#include <Parsers/IdentifierQuotingStyle.h>
#include <Poco/String.h>
#include <Common/SensitiveDataMasker.h>
#include <Common/SipHash.h>
#include <Common/StringUtils.h>

#include <algorithm>

namespace DB
{

namespace ErrorCodes
{
    extern const int TOO_BIG_AST;
    extern const int TOO_DEEP_AST;
    extern const int UNKNOWN_ELEMENT_IN_AST;
    extern const int BAD_ARGUMENTS;
}


const char * IAST::hilite_keyword      = "\033[1m";
const char * IAST::hilite_identifier   = "\033[0;36m";
const char * IAST::hilite_function     = "\033[0;33m";
const char * IAST::hilite_operator     = "\033[1;33m";
const char * IAST::hilite_alias        = "\033[0;32m";
const char * IAST::hilite_substitution = "\033[1;36m";
const char * IAST::hilite_none         = "\033[0m";


IAST::~IAST()
{
    /** Create intrusive linked list of children to delete.
      * Each ASTPtr child contains pointer to next child to delete.
      * 创建一个侵入式链接列表，用于删除子节点。
      * 每个 ASTPtr 子节点包含指向要删除的下一个子节点的指针。
      */
    ASTPtr delete_list_head_holder = nullptr;
    const bool delete_directly = next_to_delete_list_head == nullptr;
    ASTPtr & delete_list_head_reference = next_to_delete_list_head ? *next_to_delete_list_head : delete_list_head_holder;

    /// Move children into intrusive list
    /// 将子节点移动到侵入式列表中。
    for (auto & child : children)
    {
        /** If two threads remove ASTPtr concurrently,
          * it is possible that neither thead will see use_count == 1.
          * It is ok. Will need one more extra stack frame in this case.
          * 如果两个线程同时删除 ASTPtr，则可能其中一个线程看不到 use_count == 1。
          * 这是可以的。在这种情况下，需要一个额外的堆栈帧。
          */
        if (child.use_count() != 1)
            continue;

        ASTPtr child_to_delete;
        child_to_delete.swap(child);

        if (!delete_list_head_reference)
        {
            /// Initialize list first time
            delete_list_head_reference = std::move(child_to_delete);
            continue;
        }

        ASTPtr previous_head = std::move(delete_list_head_reference);
        delete_list_head_reference = std::move(child_to_delete);
        delete_list_head_reference->next_to_delete = std::move(previous_head);
    }

    if (!delete_directly)
        return;

    while (delete_list_head_reference)
    {
        /** Extract child to delete from current list head.
          * Child will be destroyed at the end of scope.
          * 从当前列表头中提取要删除的子节点。
          * 子节点将在作用域结束时销毁。
          */
        ASTPtr child_to_delete;
        child_to_delete.swap(delete_list_head_reference);

        /// Update list head
        delete_list_head_reference = std::move(child_to_delete->next_to_delete);

        /** Pass list head into child before destruction.
          * 在子节点销毁之前，将列表头传递给子节点。
          *
          * It is important to properly handle cases where subclass has member same as one of its children.
          * 重要的是要正确处理子类有成员与其中一个子节点相同的情况。
          *
          * 例如：
          * class ASTSubclass : IAST
          * {
          *     ASTPtr first_child; /// Same as first child
          * }
          *
          * In such case we must move children into list only in IAST destructor.
          * If we try to move child to delete children into list before subclasses desruction,
          * first child use count will be 2.
          * 在这种情况下，我们必须只在 IAST 析构函数中将子节点移动到列表中。
          * 如果我们尝试在子类销毁之前将子节点移动到列表中，则第一个子节点的 use_count 将为 2。
          */
        child_to_delete->next_to_delete_list_head = &delete_list_head_reference;
    }
}

/// 获取 AST 的大小。
size_t IAST::size() const
{
    size_t res = 1;
    for (const auto & child : children)
        res += child->size();

    return res;
}

/// 检查 AST 的大小。
size_t IAST::checkSize(size_t max_size) const
{
    size_t res = 1;
    for (const auto & child : children)
        res += child->checkSize(max_size);

    if (res > max_size)
        throw Exception(ErrorCodes::TOO_BIG_AST, "AST is too big. Maximum: {}", max_size);

    return res;
}

/// 获取 AST 的哈希值。
IASTHash IAST::getTreeHash(bool ignore_aliases) const
{
    SipHash hash_state;
    updateTreeHash(hash_state, ignore_aliases);
    return getSipHash128AsPair(hash_state);
}

/// 更新 AST 的哈希值。
void IAST::updateTreeHash(SipHash & hash_state, bool ignore_aliases) const
{
    updateTreeHashImpl(hash_state, ignore_aliases);
    hash_state.update(children.size());
    for (const auto & child : children)
        child->updateTreeHash(hash_state, ignore_aliases);
}

/// 更新 AST 的哈希值。
void IAST::updateTreeHashImpl(SipHash & hash_state, bool /*ignore_aliases*/) const
{
    auto id = getID();
    hash_state.update(id.data(), id.size());
}

/// 检查 AST 的深度。
size_t IAST::checkDepthImpl(size_t max_depth) const
{
    std::vector<std::pair<ASTPtr, size_t>> stack;
    stack.reserve(children.size());

    for (const auto & i: children)
        stack.push_back({i, 1});

    size_t res = 0;

    while (!stack.empty())
    {
        auto top = stack.back();
        stack.pop_back();

        if (top.second >= max_depth)
            throw Exception(ErrorCodes::TOO_DEEP_AST, "AST is too deep. Maximum: {}", max_depth);

        res = std::max(res, top.second);

        for (const auto & i: top.first->children)
            stack.push_back({i, top.second + 1});
    }

    return res;
}

/// 格式化 AST，可能隐藏敏感数据。
String IAST::formatWithPossiblyHidingSensitiveData(
    size_t max_length,
    bool one_line,
    bool show_secrets,
    bool print_pretty_type_names,
    IdentifierQuotingRule identifier_quoting_rule,
    IdentifierQuotingStyle identifier_quoting_style) const
{
    WriteBufferFromOwnString buf;
    FormatSettings settings(one_line);
    settings.show_secrets = show_secrets;
    settings.print_pretty_type_names = print_pretty_type_names;
    settings.identifier_quoting_rule = identifier_quoting_rule;
    settings.identifier_quoting_style = identifier_quoting_style;
    format(buf, settings);
    return wipeSensitiveDataAndCutToLength(buf.str(), max_length);
}

/// 格式化 AST，用于日志。
String IAST::formatForLogging(size_t max_length) const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/max_length,
        /*one_line=*/true,
        /*show_secrets=*/false,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

/// 格式化 AST，用于错误消息。
String IAST::formatForErrorMessage() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/true,
        /*show_secrets=*/false,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

/// 格式化 AST，显示敏感数据。
String IAST::formatWithSecretsOneLine() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/true,
        /*show_secrets=*/true,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

/// 格式化 AST，显示敏感数据。
String IAST::formatWithSecretsMultiLine() const
{
    return formatWithPossiblyHidingSensitiveData(
        /*max_length=*/0,
        /*one_line=*/false,
        /*show_secrets=*/true,
        /*print_pretty_type_names=*/false,
        /*identifier_quoting_rule=*/IdentifierQuotingRule::WhenNecessary,
        /*identifier_quoting_style=*/IdentifierQuotingStyle::Backticks);
}

/// 检查 AST 的子节点是否包含敏感数据。
bool IAST::childrenHaveSecretParts() const
{
    for (const auto & child : children)
    {
        if (child->hasSecretParts())
            return true;
    }
    return false;
}

void IAST::cloneChildren()
{
    for (auto & child : children)
        child = child->clone();
}

/// 获取列名。
String IAST::getColumnName() const
{
    WriteBufferFromOwnString write_buffer;
    appendColumnName(write_buffer);
    return write_buffer.str();
}

/// 获取列名，不包含别名。
String IAST::getColumnNameWithoutAlias() const
{
    WriteBufferFromOwnString write_buffer;
    appendColumnNameWithoutAlias(write_buffer);
    return write_buffer.str();
}

/// 写入标识符。
void IAST::FormatSettings::writeIdentifier(WriteBuffer & ostr, const String & name, bool ambiguous) const
{
    checkIdentifier(name);
    bool must_quote
        = (identifier_quoting_rule == IdentifierQuotingRule::Always
           || (ambiguous && identifier_quoting_rule == IdentifierQuotingRule::WhenNecessary));

    if (identifier_quoting_rule == IdentifierQuotingRule::UserDisplay && !must_quote)
    {
        // Quote `name` if it is one of the keywords when `identifier_quoting_rule` is `IdentifierQuotingRule::UserDisplay`
        const auto & keyword_set = getKeyWordSet();
        must_quote = keyword_set.contains(Poco::toUpper(name));
    }

    switch (identifier_quoting_style)
    {
        case IdentifierQuotingStyle::Backticks:
        {
            if (must_quote)
                writeBackQuotedString(name, ostr);
            else
                writeProbablyBackQuotedString(name, ostr);
            break;
        }
        case IdentifierQuotingStyle::DoubleQuotes:
        {
            if (must_quote)
                writeDoubleQuotedString(name, ostr);
            else
                writeProbablyDoubleQuotedString(name, ostr);
            break;
        }
        case IdentifierQuotingStyle::BackticksMySQL:
        {
            if (must_quote)
                writeBackQuotedStringMySQL(name, ostr);
            else
                writeProbablyBackQuotedStringMySQL(name, ostr);
            break;
        }
    }
}

/// 检查标识符。
void IAST::FormatSettings::checkIdentifier(const String & name) const
{
    if (enforce_strict_identifier_format)
    {
        bool is_word_char_identifier = std::all_of(name.begin(), name.end(), isWordCharASCII);
        if (!is_word_char_identifier)
        {
            throw Exception(
                ErrorCodes::BAD_ARGUMENTS,
                "Identifier '{}' contains characters other than alphanumeric and cannot be when enforce_strict_identifier_format is enabled",
                name);
        }
    }
}

/// 打印 AST 树。
void IAST::dumpTree(WriteBuffer & ostr, size_t indent) const
{
    String indent_str(indent, '-');
    ostr << indent_str << getID() << ", ";
    writePointerHex(this, ostr);
    writeChar('\n', ostr);
    for (const auto & child : children)
    {
        if (!child)
            throw Exception(ErrorCodes::UNKNOWN_ELEMENT_IN_AST, "Can't dump a nullptr child");
        child->dumpTree(ostr, indent + 1);
    }
}

/// 打印 AST 树。
std::string IAST::dumpTree(size_t indent) const
{
    WriteBufferFromOwnString wb;
    dumpTree(wb, indent);
    return wb.str();
}

}
