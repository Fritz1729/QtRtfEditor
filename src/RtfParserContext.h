#pragma once

#include "RtfInputReader.h"
#include "RtfTypes.h"
#include "ScopeStack.h"

#include <QtGlobal>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Rte {

/**
 * @brief All scope-pushed formatting state, tracked as one unit.
 *
 * Groups the per-group scopes so entering/leaving a group touches a single
 * object instead of six independent stacks.
 */
struct FormatScopes {
    ScopeStack<RtfRunFormat>    format;
    ScopeStack<ParagraphFormat> para;
    ScopeStack<Qt::Alignment>   tabAlign{Qt::AlignLeft};
    ScopeStack<int>             deff{0};
    ScopeStack<int>             deftab{180};   // RTF spec default: 180 twips (1/8 inch)
    ScopeStack<int>             uc{1};         // RTF spec default: 1 fallback byte after \uXXXX

    void enterScope() {
        format.enterScope();
        para.enterScope();
        tabAlign.enterScope();
        deff.enterScope();
        deftab.enterScope();
        uc.enterScope();
    }
    void leaveScope() {
        format.leaveScope();
        para.leaveScope();
        tabAlign.leaveScope();
        deff.leaveScope();
        deftab.leaveScope();
        uc.leaveScope();
    }
};

/**
 * @brief List state accumulated during parsing: current list id/level/style,
 * whether the first paragraph has been flushed, and the listtable id->style map.
 */
struct ListState {
    int listId = 0;
    int listLevel = 0;
    RtfListStyle listStyle = RtfListStyle::None;
    bool paragraphFlushed = false;
    std::map<int, RtfListStyle> listIdToStyle;
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

void SkipGroup(InputReader& input);
bool HasNonWhitespaceText(const std::vector<RtfRun>& runs);
bool ParagraphHasNonWhitespaceContent(const RtfParagraph& p);
bool TableRowHasNonWhitespaceContent(const RtfTableRowEntry& row);
void RecordUnknownTag(RtfDocument& doc, const std::string& word, int arg);
void ResetListAndTabState(FormatScopes& scopes, ListState& list);
std::pair<std::string, int> ParseControlWordWithArg(InputReader& input);

void FlushCurrentParagraph(RtfDocument& doc, RtfParagraph& currentParagraph,
                           FormatScopes& scopes, ListState& list);

void ParseColortbl(InputReader& input, RtfDocument& doc);
void ParseFonttbl(InputReader& input, RtfDocument& doc);
void ParseListtable(InputReader& input, ListState& list);
void ParsePict(InputReader& input, RtfDocument& doc, RtfParagraph& currentParagraph,
               FormatScopes& scopes, ListState& list, RtfPictState& pict);

} // namespace Rte
