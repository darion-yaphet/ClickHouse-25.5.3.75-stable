#pragma once

#include <base/types.h>
#include <Parsers/IASTHash.h>
#include <Parsers/IAST_fwd.h>
#include <Parsers/IdentifierQuotingStyle.h>
#include <Parsers/LiteralEscapingStyle.h>
#include <Common/Exception.h>
#include <Common/TypePromotion.h>

#include <set>


class SipHash;


namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

using IdentifierNameSet = std::set<String>;

class WriteBuffer;
using Strings = std::vector<String>;

/** Element of the syntax tree (hereinafter - directed acyclic graph with elements of semantics)
  * 语法树中的元素（以下称为有向无环图，其中包含语义元素）
  */
class IAST : public std::enable_shared_from_this<IAST>, public TypePromotion<IAST>
{
public:
    ASTs children;

    virtual ~IAST();
    IAST() = default;
    IAST(const IAST &) = default;
    IAST & operator=(const IAST &) = default;

    /** Get the canonical name of the column if the element is a column */
    /// 获取列的规范名称（如果元素是列）
    String getColumnName() const;

    /** Same as the above but ensure no alias names are used. This is for index analysis */
    /// 获取列的规范名称（如果元素是列），确保没有别名
    /// 用于索引分析
    String getColumnNameWithoutAlias() const;

    // 将列名附加到写入缓冲区。
    virtual void appendColumnName(WriteBuffer &) const
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Trying to get name of not a column: {}", getID());
    }

    // 将列名附加到写入缓冲区，确保没有别名。
    virtual void appendColumnNameWithoutAlias(WriteBuffer &) const
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Trying to get name of not a column: {}", getID());
    }

    /** Get the alias, if any, or the canonical name of the column, if it is not. */
    /// 获取别名，如果有的话，或者列的规范名称，如果没有的话。
    virtual String getAliasOrColumnName() const { return getColumnName(); }

    /** Get the alias, if any, or an empty string if it does not exist, or if the element does not support aliases. */
    /// 获取别名，如果有的话，或者空字符串，如果没有的话，或者元素不支持别名。
    virtual String tryGetAlias() const { return String(); }

    /** Set the alias. */
    /// 设置别名。
    virtual void setAlias(const String & /*to*/)
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Can't set alias of {} of {}", getColumnName(), getID());
    }

    /** Get the text that identifies this element. */
    /// 获取标识这个元素的文本。
    virtual String getID(char delimiter = '_') const = 0; /// NOLINT

    ASTPtr ptr() { return shared_from_this(); }

    /** Get a deep copy of the tree. Cloned object must have the same range. */
    /// 获取树的深度拷贝。克隆对象必须具有相同的范围。
    virtual ASTPtr clone() const = 0;

    /** Get hash code, identifying this element and its subtree.
     *  Hashing by default ignores aliases (e.g. identifier aliases, function aliases, literal aliases) which is
     *  useful for common subexpression elimination. Set 'ignore_aliases = false' if you don't want that behavior.
     * 获取哈希码，标识这个元素和它的子树。
     * 默认情况下，哈希忽略别名（例如标识符别名、函数别名、文字别名），这对于公共子表达式消除很有用。
     * 如果不想这样，请设置'ignore_aliases = false'。
      */
    IASTHash getTreeHash(bool ignore_aliases) const;
    void updateTreeHash(SipHash & hash_state, bool ignore_aliases) const;
    virtual void updateTreeHashImpl(SipHash & hash_state, bool ignore_aliases) const;

    void dumpTree(WriteBuffer & ostr, size_t indent = 0) const;
    std::string dumpTree(size_t indent = 0) const;

    /** Check the depth of the tree.
      * If max_depth is specified and the depth is greater - throw an exception.
      * Returns the depth of the tree.
      * 检查树的深度。
      * 如果max_depth指定并且深度大于max_depth，则抛出异常。
      * 返回树的深度。
      */
    size_t checkDepth(size_t max_depth) const
    {
        return checkDepthImpl(max_depth);
    }

    /** Get total number of tree elements
      * 获取树的总元素数
     */
    size_t size() const;

    /** Same for the total number of tree elements.
      * 检查树的元素数。
      * 如果max_size指定并且元素数大于max_size，则抛出异常。
      * 返回树的元素数。
      */
    size_t checkSize(size_t max_size) const;

    /** Get `set` from the names of the identifiers
      * 获取标识符的`set`
     */
    virtual void collectIdentifierNames(IdentifierNameSet & set) const
    {
        for (const auto & child : children)
            child->collectIdentifierNames(set);
    }

    /// 设置子节点。
    template <typename T>
    void set(T * & field, const ASTPtr & child)
    {
        if (!child)
            return;

        T * cast = dynamic_cast<T *>(child.get());
        if (!cast)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Could not cast AST subtree");

        children.push_back(child);
        field = cast;
    }


    template <typename T>
    void replace(T * & field, const ASTPtr & child)
    {
        if (!child)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Trying to replace AST subtree with nullptr");

        T * cast = dynamic_cast<T *>(child.get());
        if (!cast)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Could not cast AST subtree");

        for (ASTPtr & current_child : children)
        {
            if (current_child.get() == field)
            {
                current_child = child;
                field = cast;
                return;
            }
        }

        throw Exception(ErrorCodes::LOGICAL_ERROR, "AST subtree not found in children");
    }

    /// 设置或替换子节点。
    template <typename T>
    void setOrReplace(T * & field, const ASTPtr & child)
    {
        if (field)
            replace(field, child);
        else
            set(field, child);
    }

    /// 重置子节点。
    template <typename T>
    void reset(T * & field)
    {
        if (field == nullptr)
            return;

        auto * child = children.begin();
        while (child != children.end())
        {
            if (child->get() == field)
                break;

            child++;
        }

        if (child == children.end())
            throw Exception(ErrorCodes::LOGICAL_ERROR, "AST subtree not found in children");

        children.erase(child);
        field = nullptr;
    }

    /// After changing one of `children` elements, update the corresponding member pointer if needed.
    /// 在更改 `children` 中的一个元素后，如果需要，更新相应的成员指针。
    void updatePointerToChild(void * old_ptr, void * new_ptr)
    {
        forEachPointerToChild([old_ptr, new_ptr](void ** ptr) mutable
        {
            if (*ptr == old_ptr)
                *ptr = new_ptr;
        });
    }

    /// Convert to a string.
    /// 转换为字符串。
    /// Format settings.
    struct FormatSettings
    {
        bool one_line;
        bool hilite;
        IdentifierQuotingRule identifier_quoting_rule;
        IdentifierQuotingStyle identifier_quoting_style;
        bool show_secrets; /// Show secret parts of the AST (e.g. passwords, encryption keys).
        char nl_or_ws; /// Newline or whitespace.
        LiteralEscapingStyle literal_escaping_style;
        bool print_pretty_type_names;
        bool enforce_strict_identifier_format;

        /// 构造函数。
        explicit FormatSettings(
            bool one_line_,
            bool hilite_ = false,
            IdentifierQuotingRule identifier_quoting_rule_ = IdentifierQuotingRule::WhenNecessary,
            IdentifierQuotingStyle identifier_quoting_style_ = IdentifierQuotingStyle::Backticks,
            bool show_secrets_ = true,
            LiteralEscapingStyle literal_escaping_style_ = LiteralEscapingStyle::Regular,
            bool print_pretty_type_names_ = false,
            bool enforce_strict_identifier_format_ = false)
            : one_line(one_line_)
            , hilite(hilite_)
            , identifier_quoting_rule(identifier_quoting_rule_)
            , identifier_quoting_style(identifier_quoting_style_)
            , show_secrets(show_secrets_)
            , nl_or_ws(one_line ? ' ' : '\n')
            , literal_escaping_style(literal_escaping_style_)
            , print_pretty_type_names(print_pretty_type_names_)
            , enforce_strict_identifier_format(enforce_strict_identifier_format_)
        {
        }

        void writeIdentifier(WriteBuffer & ostr, const String & name, bool ambiguous) const;
        void checkIdentifier(const String & name) const;
    };

    /// State. For example, a set of nodes can be remembered, which we already walk through.
    /// 状态。例如，可以记住已经遍历的节点集。
    struct FormatState
    {
        /** The SELECT query in which the alias was found; identifier of a node with such an alias.
          * It is necessary that when the node has met again, output only the alias.
          */
        std::set<std::tuple<
            const IAST * /* SELECT query node */,
            std::string /* alias */,
            IASTHash /* printed content */>> printed_asts_with_alias;
    };

    /// The state that is copied when each node is formatted. For example, nesting level.
    /// 当每个节点被格式化时，复制的状态。例如，嵌套级别。
    struct FormatStateStacked
    {
        UInt16 indent = 0;
        bool need_parens = false;
        bool expression_list_always_start_on_new_line = false;  /// Line feed and indent before expression list even if it's of single element.
        bool expression_list_prepend_whitespace = false; /// Prepend whitespace (if it is required)
        bool surround_each_list_element_with_parens = false;
        bool ignore_printed_asts_with_alias = false; /// Ignore FormatState::printed_asts_with_alias
        bool allow_operators = true; /// Format some functions, such as "plus", "in", etc. as operators.
        size_t list_element_index = 0;
        std::string create_engine_name;
        const IAST * current_select = nullptr;
    };

    void format(WriteBuffer & ostr, const FormatSettings & settings) const
    {
        FormatState state;
        formatImpl(ostr, settings, state, FormatStateStacked());
    }

    void format(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const
    {
        formatImpl(ostr, settings, state, std::move(frame));
    }

    /// TODO: Move more logic into this class (see https://github.com/ClickHouse/ClickHouse/pull/45649).
    /// 将更多逻辑移动到这个类中（见 https://github.com/ClickHouse/ClickHouse/pull/45649）。
    struct FormattingBuffer
    {
        WriteBuffer & ostr;
        const FormatSettings & settings;
        FormatState & state;
        FormatStateStacked frame;
    };

    void format(FormattingBuffer out) const
    {
        formatImpl(out.ostr, out.settings, out.state, out.frame);
    }

    /// Secrets are displayed regarding show_secrets, then SensitiveDataMasker is applied.
    /// You can use Interpreters/formatWithPossiblyHidingSecrets.h for convenience.
    /// 根据 show_secrets 显示敏感数据，然后应用 SensitiveDataMasker。
    /// 您可以使用 Interpreters/formatWithPossiblyHidingSecrets.h 方便地使用。
    String formatWithPossiblyHidingSensitiveData(
        size_t max_length,
        bool one_line,
        bool show_secrets,
        bool print_pretty_type_names,
        IdentifierQuotingRule identifier_quoting_rule,
        IdentifierQuotingStyle identifier_quoting_style) const;

    /** formatForLogging and formatForErrorMessage always hide secrets. This inconsistent
      * behaviour is due to the fact such functions are called from Client which knows nothing about
      * access rights and settings. Moreover, the only use case for displaying secrets are backups,
      * and backup tools use only direct input and ignore logs and error messages.
      */
    /// formatForLogging 和 formatForErrorMessage 总是隐藏敏感数据。
    /// 这种不一致的行为是由于这样的函数是从 Client 调用的，它不知道访问权限和设置。
    /// 此外，唯一显示敏感数据的情况是备份，而备份工具只直接输入并忽略日志和错误消息。
    String formatForLogging(size_t max_length = 0) const;
    String formatForErrorMessage() const;
    /// 格式化带有敏感数据的单行。
    String formatWithSecretsOneLine() const;
    String formatWithSecretsMultiLine() const;

    virtual bool hasSecretParts() const { return childrenHaveSecretParts(); }

    void cloneChildren();

    /// 查询类型。
    enum class QueryKind : uint8_t
    {
        None = 0,
        Select,
        Insert,
        Delete,
        Update,
        Create,
        Drop,
        Undrop,
        Rename,
        Optimize,
        Check,
        Alter,
        Grant,
        Revoke,
        Move,
        System,
        Set,
        Use,
        Show,
        Exists,
        Describe,
        Explain,
        Backup,
        Restore,
        KillQuery,
        ExternalDDL,
        Begin,
        Commit,
        Rollback,
        SetTransactionSnapshot,
        AsyncInsertFlush,
        ParallelWithQuery,
    };

    /// Return QueryKind of this AST query.
    virtual QueryKind getQueryKind() const { return QueryKind::None; }

    /// For syntax highlighting.
    /// 用于语法高亮。
    static const char * hilite_keyword;
    static const char * hilite_identifier;
    static const char * hilite_function;
    static const char * hilite_operator;
    static const char * hilite_alias;
    static const char * hilite_substitution;
    static const char * hilite_none;

protected:
    /// 格式化实现。
    virtual void formatImpl(WriteBuffer & ostr,
                            const FormatSettings & settings,
                            FormatState & state,
                            FormatStateStacked frame) const
    {
        formatImpl(FormattingBuffer{ostr, settings, state, std::move(frame)});
    }

    /// 格式化实现。
    virtual void formatImpl(FormattingBuffer /*out*/) const
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unknown element in AST: {}", getID());
    }

    /// 子节点是否包含敏感部分。
    bool childrenHaveSecretParts() const;

    /// 某些 AST 类有裸指针到子元素作为成员。
    /// Some AST classes have naked pointers to children elements as members.
    /// This method allows to iterate over them.
    /// 某些 AST 类有裸指针到子元素作为成员。
    /// 此方法允许迭代它们。
    virtual void forEachPointerToChild(std::function<void(void**)>) {}

private:
    /// 检查深度实现。
    size_t checkDepthImpl(size_t max_depth) const;

    /** Forward linked list of ASTPtr to delete.
      * 前向链接列表的 ASTPtr 到删除。
      *
      * Used in IAST destructor to avoid possible stack overflow.
      * 用于在 IAST 析构函数中避免可能的堆栈溢出。
      */
    ASTPtr next_to_delete = nullptr;
    ASTPtr * next_to_delete_list_head = nullptr;
};

}
