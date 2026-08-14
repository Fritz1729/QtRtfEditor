#pragma once

#include "RtfInputReader.h"
#include "RtfTypes.h"

#include <QtGlobal>
#include <cctype>
#include <map>
#include <utility>
#include <vector>

namespace Rte {

template<typename T>
struct ScopeStack {
    std::vector<T> stack;
    T current;

    ScopeStack() = default;
    explicit ScopeStack(T initial) : current(std::move(initial)) {}

    void enterScope() { stack.push_back(current); }
    void leaveScope() {
        if (!stack.empty()) {
            current = std::move(stack.back());
            stack.pop_back();
        }
    }
    const T& get() const { return current; }
    T& get() { return current; }
};

[[nodiscard]] constexpr bool IsPrintable(char c) {
    return std::isprint(static_cast<unsigned char>(c));
}

struct RtfParserContext {
    InputReader& input;
    RtfDocument& doc;
    size_t& iter;
};

struct RtfListtableContext {
    InputReader& input;
    size_t& iter;
    std::map<int, RtfListStyle>& listIdToStyle;
};

struct RtfPictState {
    std::string data;
    std::string format;
    int picw = 0;
    int pich = 0;
    int picwgoal = 0;
    int pichgoal = 0;
    int picscalex = 100;
    int picscaley = 100;
    int piccropl = 0;
    int piccropr = 0;
    int piccropt = 0;
    int piccropb = 0;
};

struct RtfScopeStacks {
    ScopeStack<RtfRunFormat>& formatScope;
    ScopeStack<ParagraphFormat>& paraScope;
    ScopeStack<Qt::Alignment>& tabAlignScope;
    ScopeStack<int>& deffScope;
    ScopeStack<int>& deftabScope;
    ScopeStack<int>& ucScope;
};

struct RtfListState {
    int& listId;
    int& listLevel;
    RtfListStyle& listStyle;
    bool& paragraphFlushed;
    std::map<int, RtfListStyle>& listIdToStyle;
};

struct RtfParserScopeContext {
    InputReader& input;
    RtfDocument& doc;
    RtfParagraph& currentParagraph;
    size_t& iter;

    RtfScopeStacks scopes;
    RtfListState listState;
    RtfPictState pict;

    RtfParserScopeContext(InputReader& inp, RtfDocument& d, RtfParagraph& cp,
                           size_t& it, RtfScopeStacks s, RtfListState ls)
        : input(inp), doc(d), currentParagraph(cp), iter(it),
          scopes(s), listState(ls) {}
};

void CheckIter(size_t& iter);
void SkipGroup(InputReader& input, size_t& iter);
bool ParagraphHasNonWhitespaceContent(const RtfParagraph& p);
std::pair<std::string, int> ParseControlWordWithArg(InputReader& input);

void FlushCurrentParagraph(RtfParserScopeContext& ctx);

void ParseColortbl(RtfParserContext& ctx);
void ParseFonttbl(RtfParserContext& ctx);
void ParseListtable(RtfListtableContext& ctx);
void ParsePict(RtfParserScopeContext& ctx);

} // namespace Rte
