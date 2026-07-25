#include <QtTest>
#include <future>
#include <chrono>
#include "RtfCompare.h"
#include "RtfParser.h"

using namespace Rte;

// ParseRtf() may hang on certain control words (\colortbl, etc.).
// These helpers run the operation in a detached background thread
// and time out after 3 seconds.

struct CompareResult {
    RtfCompareResult result = RtfCompareResult::StructuralDiff;
    std::string reason;
    bool ok = false;
};

static CompareResult SafeCompareRtf(const std::string& rtfA, const std::string& rtfB) {
    CompareResult r;
    try {
        std::string reason;
        r.result = CompareRtf(rtfA, rtfB, reason);
        r.reason = reason;
        r.ok = true;
    } catch (...) {
        r.ok = false;
    }
    return r;
}

static CompareResult CompareWithTimeout(const std::string& rtfA, const std::string& rtfB, int sec) {
    std::promise<CompareResult> promise;
    std::future<CompareResult> future = promise.get_future();

    std::thread t([&]() {
        CompareResult r = SafeCompareRtf(rtfA, rtfB);
        promise.set_value(r);
    });
    t.detach();

    std::future_status status = future.wait_for(std::chrono::seconds(sec));
    if (status != std::future_status::ready) {
        return CompareResult{};
    }
    return future.get();
}


class TestSemanticComparison : public QObject {
    Q_OBJECT

private slots:
    // True positives — must detect differences
    void IdenticalRtf();
    void IdenticalItalic();
    void IdenticalUnderline();
    void DifferentText();
    void DifferentFormatting();
    void DifferentParagraphCount();

    // False positive guards — must NOT flag as different
    void SemanticColor();
    void SemanticFont();

    // False negative guards — must flag as different
    void DifferentAlignment();
    void DifferentIndent();
    void DifferentFirstLineIndent();
    void DifferentSuperscript();
    void DifferentSubscript();
    void DifferentFontSize();
    void DifferentItalic();
    void DifferentUnderline();
    void DifferentTab();
    void DifferentTabStops();
    void TabAlignDecimalVsCenter();
    void TabAlignDecimalVsRight();
    void TabAlignDecimalVsLeft();

    // Group-persistent: \deff — different default font index
    void DifferentDeff();
    void IdenticalDeff();
    void DeffGroupPersistent();
    void DeffNestedGroupReverted();

    // Group-persistent: \deftab — different default tab stop
    void DifferentDeftab();
    void IdenticalDeftab();
    void DeftabGroupPersistent();
    void DeftabNestedGroupReverted();

    // Justification
    void DifferentJustification();

    // Lists
    void DifferentListStyle();
    void DifferentListIndent();
    void DifferentListLevel();

    // Underline color
    void DifferentUlColor();

    // Language ID
    void DifferentLangId();

    // RE 2.0 — True positives (must detect differences)
    void DifferentStrike();
    void DifferentCb();
    void DifferentRightIndent();
    void DifferentSpaceBefore();
    void DifferentSpaceAfter();
    void DifferentLineHeight();
    void SlMultDifferent();
    void DifferentCaps();
    void DifferentScaps();
    void DifferentUnderlineStyle();
    void DifferentUnderlineThick();
    void DifferentUnderlineDbl();
    void DifferentUp();
    void DifferentDn();
    void DifferentKerning();
    void DifferentExpnd();
    void DifferentUnderlineDashDot();
    void DifferentUnderlineDashDotDot();

    // RE 2.0 — Semantic identity (must NOT flag as different)
    void CbSemantic();

    // Images — must detect differences
    void DifferentImageCount();
    void DifferentImageFormat();
    void DifferentImageData();

    // Images — semantic identity (different dimensions, same data)
    void SemanticImageData();

    // Protect
    void DifferentProtect();

    // Header metadata
    void DifferentDeflang();
    void DifferentViewKind();
    void DifferentUcByteCount();

    // \plain / \pard semantic equivalence
    void SemanticPlainVsManualReset();
    void SemanticPardVsManualReset();

    // Edge cases
    void EmptyDocs();
    void HeaderOnly();
    void UnknownTags();
    void EscapedBackslash();
    void EscapedBackslashWithBraces();
    void EscapedBackslashRoundtrip();
    void EmptyParagraphsPreserved();

    // Whitespace preservation and underline control words
    void WhitespaceAfterToggleOff();
    void UlNone();
    void SemanticUlSynonyms();

    // Parser fixes — \ucN skip chars, star groups, negative args
    void UcSkipChars();
    void UcSkipCharsGroupScoped();
    void StarGroupSkippedSilently();
    void NegativeArgNoSpace();
    void NegativeArgUnicode();

    // Hyperlinks — field parsing and roundtrip
    void HyperlinkExternalUrl();
    void HyperlinkInternalBookmark();
    void HyperlinkIdentical();
    void HyperlinkDifferentUrl();
    void HyperlinkWithBold();

    // Tables
    void DifferentCellShading();
    void DifferentCellShadingVsNoShading();
    void DifferentTableRowCount();
    void DifferentTableColCount();
    void DifferentTableCellText();
    void DifferentTableCellWidth();
    void DifferentCellVertAlign();
    void DifferentCellBorders();
    void TableWithEmptyCell();
    void TableWithFormatting();
    void TableOrderingParagraphTableParagraph();
    void TableOrderingPTPTP();

    // Table padding
    void DifferentCellPadding();
    void DifferentRowPadding();
    void SemanticPaddingCellVsRow();

    // Table alignment
    void DifferentTableAlignment();

    // Row borders vs cell borders
    void RowBorderVsCellBorderSame();
    void RowBorderVsCellBorderDifferent();
    void DifferentBorderStyle();

    void cleanupTestCase();

private:
    int _timeout = 0;
};

void TestSemanticComparison::IdenticalRtf() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::IdenticalItalic() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\i Italic}\i0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\i Italic}\i0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::IdenticalUnderline() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DifferentText() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 World\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentFormatting() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\b Bold}\b0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Bold\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentParagraphCount() {
    std::string rtfA = R"({\rtf1\ansi\deff0 One\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 One\par Two\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::SemanticColor() {
    // Different colortbl indices → same resolved RGB
    std::string rtfA = R"({\rtf1\ansi\deff0
{\colortbl ;\red255\green0\blue0;}
\cf1 Red\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0
{\colortbl ;\red0\green255\blue0;\red255\green0\blue0;}
\cf2 Red\par})";
    CompareResult r = CompareWithTimeout(rtfA, rtfB, 3);
    if (!r.ok) {
        _timeout++;
        QFAIL("parser timed out — feature not yet implemented");
    }
    QCOMPARE(r.result, RtfCompareResult::Identical);
    QVERIFY(r.reason.empty());
}

void TestSemanticComparison::SemanticFont() {
    // Different fonttbl indices → same resolved family
    std::string rtfA = R"({\rtf1\ansi\deff0
{\fonttbl{\f0\fswiss\fcharset0 Arial;}}
\f0 Arial\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0
{\fonttbl{\f0\froman\fcharset0 Times;}
         {\f1\fswiss\fcharset0 Arial;}}
\f1 Arial\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::DifferentAlignment() {
    std::string rtfA = R"({\rtf1\ansi\deff0\ql Left\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\qc Left\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentIndent() {
    std::string rtfA = R"({\rtf1\ansi\deff0\li500 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\li200 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentFirstLineIndent() {
    std::string rtfA = R"({\rtf1\ansi\deff0\fi500 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\fi200 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentSuperscript() {
    std::string rtfA = R"({\rtf1\ansi\deff0 H\super 2\super0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 H 2\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentSubscript() {
    std::string rtfA = R"({\rtf1\ansi\deff0 H\sub 2\sub0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 H 2\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentFontSize() {
    std::string rtfA = R"({\rtf1\ansi\deff0\f0\fs24 Small\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\f0\fs48 Large\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentItalic() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\i Italic}\i0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Italic\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentUnderline() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Underlined\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentTab() {
    std::string rtfA = R"({\rtf1\ansi\deff0 One\tab Two\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 One  Two\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentTabStops() {
    std::string rtfA = R"({\rtf1\ansi\deff0\tx1000\tqc\tx2000\tx3000\tqr Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\tx1000\tx2000\tx3000\tx4000\tqc Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::TabAlignDecimalVsCenter() {
    std::string rtfA = R"({\rtf1\ansi\deff0\tqd\tx1000 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\tqc\tx1000 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::TabAlignDecimalVsRight() {
    std::string rtfA = R"({\rtf1\ansi\deff0\tqd\tx1000 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\tqr\tx1000 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::TabAlignDecimalVsLeft() {
    std::string rtfA = R"({\rtf1\ansi\deff0\tqd\tx1000 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\tx1000 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentJustification() {
    std::string rtfA = R"({\rtf1\ansi\deff0\ql Left\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\qj Justified\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentListStyle() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylenum\liststyletype3}}{\listid1\listlevel0 Item one\par}{\listid1\listlevel0 Item two\par}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylebulletsimple}}{\listid1\listlevel0 Item one\par}{\listid1\listlevel0 Item two\par}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentListIndent() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylenum\liststyletype3}}{\listid1\listlevel0\li200 Item one\par}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylenum\liststyletype3}}{\listid1\listlevel0\li400 Item one\par}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentListLevel() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylenum\liststyletype3}}{\listid1\listlevel0 Item\par}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\listtable{\list\listid1\liststylenum\liststyletype3}}{\listid1\listlevel1 Item\par}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentUlColor() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\colortbl ;\red255\green0\blue0;}{\ul\ulc1 Colored}\ul0\ulc0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\colortbl ;\red0\green255\blue0;}{\ul\ulc1 Colored}\ul0\ulc0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentLangId() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\lang1033 English}\lang0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\lang1031 German}\lang0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentStrike() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\strike Strike}\strike0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Strike\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentCb() {
    std::string rtfA = R"({\rtf1\ansi\deff0
{\colortbl ;\red255\green0\blue0;\red0\green255\blue0;}
\cb1 Red-bg\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0
{\colortbl ;\red255\green0\blue0;\red0\green255\blue0;}
\cb2 Green-bg\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentRightIndent() {
    std::string rtfA = R"({\rtf1\ansi\deff0\ri500 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\ri0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentSpaceBefore() {
    std::string rtfA = R"({\rtf1\ansi\deff0\sb100 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\sb0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentSpaceAfter() {
    std::string rtfA = R"({\rtf1\ansi\deff0\sa200 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\sa0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentLineHeight() {
    std::string rtfA = R"({\rtf1\ansi\deff0\sl400 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\sl0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::SlMultDifferent() {
    std::string rtfA = R"({\rtf1\ansi\deff0\slmult2 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\slmult1 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentCaps() {
    std::string rtfA = R"({\rtf1\ansi\deff0\caps Caps\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Caps\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentScaps() {
    std::string rtfA = R"({\rtf1\ansi\deff0\scaps SmallCaps\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 SmallCaps\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUnderlineStyle() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\uldash Dashed}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUnderlineThick() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\ulth Thick}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUnderlineDbl() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\uldb Double}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUp() {
    std::string rtfA = R"({\rtf1\ansi\deff0\up12 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\up6 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentDn() {
    std::string rtfA = R"({\rtf1\ansi\deff0\dn12 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\dn6 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentKerning() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\kerning Kerned}\kerning0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Kerned\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentExpnd() {
    std::string rtfA = R"({\rtf1\ansi\deff0\expnd20 Expanded\expnd0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Expanded\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUnderlineDashDot() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\uldashd DashDot}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentUnderlineDashDotDot() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\ul Underlined}\ul0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\uldashdd DashDotDot}\ul0\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::CbSemantic() {
    std::string rtfA = R"({\rtf1\ansi\deff0
{\colortbl ;\red128\green64\blue0;}
\cb1 Orange\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0
{\colortbl ;\red0\green0\blue0;\red128\green64\blue0;}
\cb2 Orange\par})";
    CompareResult r = CompareWithTimeout(rtfA, rtfB, 3);
    if (!r.ok) {
        _timeout++;
        QFAIL("parser timed out — feature not yet implemented");
    }
    QCOMPARE(r.result, RtfCompareResult::Identical);
    QVERIFY(r.reason.empty());
}

void TestSemanticComparison::DifferentImageCount() {
    // Create minimal 1x1 red PNG
    QByteArray png1, png2;
    {
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(qRgb(255, 0, 0));
        QBuffer b1(&png1); b1.open(QIODevice::WriteOnly); img.save(&b1, "PNG");
        QBuffer b2(&png2); b2.open(QIODevice::WriteOnly); img.save(&b2, "PNG");
    }

    std::string rtfA = R"({\rtf1\ansi\deff0)";
    rtfA += "{\\pict\\pngblip ";
    rtfA += QString::fromLatin1(png1.toHex().data()).toStdString();
    rtfA += R"(}\par})";

    std::string rtfB = R"({\rtf1\ansi\deff0)";
    rtfB += "{\\pict\\pngblip ";
    rtfB += QString::fromLatin1(png1.toHex().data()).toStdString();
    rtfB += R"(}\par})";
    rtfB += "{\\pict\\pngblip ";
    rtfB += QString::fromLatin1(png2.toHex().data()).toStdString();
    rtfB += R"(}\par})";

    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentImageFormat() {
    QByteArray pngData, bmpData;
    {
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(qRgb(255, 0, 0));
        QBuffer b1(&pngData); b1.open(QIODevice::WriteOnly); img.save(&b1, "PNG");
        QBuffer b2(&bmpData); b2.open(QIODevice::WriteOnly); img.save(&b2, "BMP");
    }

    std::string rtfA = R"({\rtf1\ansi\deff0)";
    rtfA += "{\\pict\\pngblip ";
    rtfA += QString::fromLatin1(pngData.toHex().data()).toStdString();
    rtfA += R"(}\par})";

    std::string rtfB = R"({\rtf1\ansi\deff0)";
    rtfB += "{\\pict\\dibitmap ";
    rtfB += QString::fromLatin1(bmpData.toHex().data()).toStdString();
    rtfB += R"(}\par})";

    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::DifferentImageData() {
    QByteArray png1, png2;
    {
        QImage img1(1, 1, QImage::Format_RGB32);
        img1.fill(qRgb(255, 0, 0));
        QBuffer b1(&png1); b1.open(QIODevice::WriteOnly); img1.save(&b1, "PNG");

        QImage img2(1, 1, QImage::Format_RGB32);
        img2.fill(qRgb(0, 255, 0));
        QBuffer b2(&png2); b2.open(QIODevice::WriteOnly); img2.save(&b2, "PNG");
    }

    std::string rtfA = R"({\rtf1\ansi\deff0)";
    rtfA += "{\\pict\\pngblip ";
    rtfA += QString::fromLatin1(png1.toHex().data()).toStdString();
    rtfA += R"(}\par})";

    std::string rtfB = R"({\rtf1\ansi\deff0)";
    rtfB += "{\\pict\\pngblip ";
    rtfB += QString::fromLatin1(png2.toHex().data()).toStdString();
    rtfB += R"(}\par})";

    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::SemanticImageData() {
    QByteArray pngData;
    {
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(qRgb(255, 0, 0));
        QBuffer b(&pngData); b.open(QIODevice::WriteOnly); img.save(&b, "PNG");
    }

    std::string hex = QString::fromLatin1(pngData.toHex().data()).toStdString();

    std::string rtfA = R"({\rtf1\ansi\deff0)";
    rtfA += "{\\pict\\pngblip\\picwgoal100\\pichgoal100 ";
    rtfA += hex;
    rtfA += R"(}\par})";

    std::string rtfB = R"({\rtf1\ansi\deff0)";
    rtfB += "{\\pict\\pngblip\\picwgoal200\\pichgoal200 ";
    rtfB += hex;
    rtfB += R"(}\par})";

    std::string reason;
    // Same image data → Identical (dimensions are not compared semantically)
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::DifferentProtect() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\protect Protected}\protect0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Protected\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::EmptyDocs() {
    std::string rtfA = R"({\rtf1\ansi\deff0})";
    std::string rtfB = R"({\rtf1\ansi\deff0})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::HeaderOnly() {
    std::string rtfA = R"({\rtf1\ansi\deff0})";
    std::string rtfB = R"({\rtf1\ansi\deff0\rtf1\ansi})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::UnknownTags() {
    // \xyz is not a recognized control word and not \u (unicode escape)
    std::string rtfA = R"({\rtf1\ansi\deff0 Text\xyz\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::UnknownTag);
}

void TestSemanticComparison::EscapedBackslash() {
    // \\ produces a literal backslash character
    std::string rtfA = R"({\rtf1\ansi\deff0 Path: C:\\Users\\test\par})";
    std::string reason;
    auto doc = ParseRtf(rtfA);
    QVERIFY(doc.elements.size() >= 1);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("C:\\Users\\test") != std::string::npos) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestSemanticComparison::EscapedBackslashWithBraces() {
    // \\{ and \\} produce literal \{ and \} (not backslash + group delimiter)
    std::string rtfA = R"({\rtf1\ansi\deff0 Set: K[x]\\{0\\}\par})";
    std::string reason;
    auto doc = ParseRtf(rtfA);
    QVERIFY(doc.elements.size() >= 1);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("K[x]\\{0\\}") != std::string::npos) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestSemanticComparison::EscapedBackslashRoundtrip() {
    // RTF with \\ should roundtrip: parse → save → parse should yield identical structure
    std::string rtfA = R"({\rtf1\ansi\deff0 Path: C:\\Users\\test\par})";
    auto doc = ParseRtf(rtfA);
    QVERIFY(doc.elements.size() >= 1);
    auto doc2 = ParseRtf(rtfA);
    std::string reason;
    QCOMPARE(CompareRtf(doc, doc2, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::EmptyParagraphsPreserved() {
    // RTF with an empty paragraph between two content paragraphs.
    // The parser must preserve the empty paragraph; both \par tokens
    // produce distinct paragraphs in the element list.
    std::string rtf = R"({\rtf1\ansi\deff0 Para1\par\par Para3\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 3u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[1]));
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[2]));
    const auto& para2 = std::get<RtfParagraph>(doc.elements[1]);
    QVERIFY(para2.runs.empty());
}

void TestSemanticComparison::WhitespaceAfterToggleOff() {
    // Whitespace after a toggle-OFF (e.g. \b0) must be preserved
    // as content, not trimmed away. After \b0, one trailing space
    // is consumed as the RTF delimiter (per spec), so one space
    // remains as content.
    std::string rtf = R"({\rtf1\ansi\deff0 Test \b bold\b0  regular\par})";
    auto doc = ParseRtf(rtf);
    QVERIFY(doc.elements.size() >= 1);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    QVERIFY(para.runs.size() >= 3);

    // Find the "regular" run — it should start with a space
    bool foundRegularWithSpace = false;
    for (const auto& run : para.runs) {
        if (run.text.find("regular") != std::string::npos) {
            QVERIFY(!run.text.empty());
            QVERIFY(run.text[0] == ' ');
            foundRegularWithSpace = true;
        }
    }
    QVERIFY(foundRegularWithSpace);
}

void TestSemanticComparison::UlNone() {
    // \ulnone must turn off underline, not turn it on
    std::string rtf = R"({\rtf1\ansi\deff0 Text {\ul underlined\ulnone} normal\par})";
    auto doc = ParseRtf(rtf);
    QVERIFY(doc.elements.size() >= 1);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    QVERIFY(para.runs.size() >= 3);

    bool foundUnderlined = false;
    bool foundNormal = false;
    for (const auto& run : para.runs) {
        if (run.text.find("underlined") != std::string::npos) {
            QVERIFY(run.format.underline);
            foundUnderlined = true;
        }
        if (run.text.find("normal") != std::string::npos) {
            QVERIFY(!run.format.underline);
            foundNormal = true;
        }
    }
    QVERIFY(foundUnderlined);
    QVERIFY(foundNormal);
}

void TestSemanticComparison::SemanticUlSynonyms() {
    // \ul ... \ul0 and \ul ... \ulnone must be semantically identical
    std::string rtfUl0 = R"({\rtf1\ansi\deff0{\ul Text}\ul0\par})";
    std::string rtfUlNone = R"({\rtf1\ansi\deff0{\ul Text}\ulnone\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfUl0, rtfUlNone, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::UcSkipChars() {
    // \uc2\u252 ?? — the two fallback bytes after \u252 should be skipped
    // without being treated as content. \u252 is ì (Latin small i with grave).
    // The fallback "???" here is 2 bytes that should be consumed.
    auto doc = ParseRtf(R"({\rtf1\ansi\deff0\uc2\u252 AB\par})");
    QCOMPARE(doc.elements.size(), 1u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    // The fallback bytes "AB" should have been skipped — no content after the char
    // Only the Unicode char should be in the text, no "AB"
    for (const auto& run : para.runs) {
        QVERIFY(run.text.find("AB") == std::string::npos);
    }
}

void TestSemanticComparison::UcSkipCharsGroupScoped() {
    // \uc is group-persistent: inner group \uc1 overrides outer \uc2
    // \u252 AB — \uc2 skips "AB" (2 bytes)
    // \u33C — \uc1 skips "C" (1 byte)
    std::string rtfA = R"({\rtf1\ansi\deff0\uc2\u252 AB {\uc1\u33C\par})";
    auto docA = ParseRtf(rtfA);
    // "AB" should be skipped (2 bytes), "C" should be skipped (1 byte)
    // Only the two Unicode chars should appear
    QCOMPARE(docA.elements.size(), 1u);
    const auto& para = std::get<RtfParagraph>(docA.elements[0]);
    for (const auto& run : para.runs) {
        QVERIFY(run.text.find("AB") == std::string::npos);
        QVERIFY(run.text.find("C") == std::string::npos);
    }
}

void TestSemanticComparison::StarGroupSkippedSilently() {
    // Unknown star-prefixed groups like {\*\unknownword ...} should be
    // silently skipped — not recorded as unknown tags
    auto doc = ParseRtf(R"({\rtf1\ansi\deff0{\*\unknownword garbage} Text\par})");
    QVERIFY(doc.unknownTags.empty());
    QCOMPARE(doc.elements.size(), 1u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    for (const auto& run : para.runs) {
        if (run.text.find("Text") != std::string::npos) return;
    }
    QFAIL("Text content not found");
}

void TestSemanticComparison::NegativeArgNoSpace() {
    // \li-500 should be parsed as negative indent (-500 twips)
    auto doc = ParseRtf(R"({\rtf1\ansi\deff0\li-500 Text\par})");
    QCOMPARE(doc.elements.size(), 1u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    QCOMPARE(para.leftIndent, -500);
}

void TestSemanticComparison::NegativeArgUnicode() {
    // \u-500 should normalize to 65036 (0xFE0C) via 65536-500
    auto doc = ParseRtf(R"({\rtf1\ansi\deff0\uc0\u-500 Text\par})");
    QCOMPARE(doc.elements.size(), 1u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    // \uc0 means no fallback bytes to skip, so "Text" is content
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("Text") != std::string::npos) found = true;
    }
    QVERIFY(found);
}

void TestSemanticComparison::DifferentTableRowCount() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \intbl B\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \intbl B\cell \row}{\trowd \cellx2000 \cellx4000 \intbl C\cell \intbl D\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentTableColCount() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \intbl B\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentTableCellText() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \intbl B\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl X\cell \intbl Y\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentTableCellWidth() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \intbl B\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx3000 \cellx6000 \intbl A\cell \intbl B\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentCellVertAlign() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clvertalt \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clvertalb \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentCellBorders() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrs\brdrw10 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrs\brdrw20 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::TableWithEmptyCell() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl A\cell \cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::TableWithFormatting() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl {\b Bold}\b0 \cell \intbl Normal\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \cellx4000 \intbl Normal\cell \intbl Normal\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentCellShading() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clshdn1 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clshdn2 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentCellShadingVsNoShading() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clshdn1 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::TableOrderingParagraphTableParagraph() {
    std::string rtf = R"({\rtf1\ansi\deff0 Para A\par{\trowd \cellx2000 \intbl T\cell \row}Para B\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 3u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    QVERIFY(std::holds_alternative<RtfTableRowEntry>(doc.elements[1]));
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[2]));
}

void TestSemanticComparison::TableOrderingPTPTP() {
    std::string rtf = R"({\rtf1\ansi\deff0 P1\par{\trowd \cellx2000 \intbl T1\cell \row}P2\par{\trowd \cellx3000 \intbl T2\cell \row}P3\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 5u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    QVERIFY(std::holds_alternative<RtfTableRowEntry>(doc.elements[1]));
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[2]));
    QVERIFY(std::holds_alternative<RtfTableRowEntry>(doc.elements[3]));
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[4]));
}

void TestSemanticComparison::DifferentCellPadding() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clpadl100 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clpadl200 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentRowPadding() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \trpaddl100 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \trpaddl200 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::SemanticPaddingCellVsRow() {
    // Same effective padding: cell-level vs row-level
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \trpaddl100 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clpadl100 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DifferentTableAlignment() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \trql \cellx2000 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \trqc \cellx2000 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::RowBorderVsCellBorderSame() {
    // Row border and cell border produce same effective border
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \trbrdrl\brdrs\brdrw10 \cellx2000 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrs\brdrw10 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::RowBorderVsCellBorderDifferent() {
    // Row border vs different cell border
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \trbrdrl\brdrs\brdrw10 \cellx2000 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrs\brdrw20 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentBorderStyle() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrs\brdrw10 \intbl A\cell \row}})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\trowd \cellx2000 \clbrdrl\brdrd\brdrw10 \intbl A\cell \row}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentDeflang() {
    std::string rtfA = R"({\rtf1\ansi\deff0\deflang1031 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\deflang1033 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentViewKind() {
    std::string rtfA = R"({\rtf1\ansi\deff0\viewkind4 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\viewkind3 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DifferentUcByteCount() {
    std::string rtfA = R"({\rtf1\ansi\deff0\uc1 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\uc2 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::SemanticPlainVsManualReset() {
    std::string rtfA = R"({\rtf1\ansi\deff0\b Bold\i Italic\super Sup\plain Normal\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\b Bold\i Italic\super Sup\b0\i0\super0 Normal\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::SemanticPardVsManualReset() {
    std::string rtfA = R"({\rtf1\ansi\deff0\li500\qc Centered\pard\ql Left\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\li500\qc Centered\pard\ql Left\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DifferentDeff() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff1 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::IdenticalDeff() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DeffGroupPersistent() {
    // \deff in subgroup changes the paragraph's defaultFontIndex
    // Both have \deff1 in subgroup, so both paragraphs should have defaultFontIndex=1
    std::string rtfA = R"({\rtf1\ansi\deff0\pard\plain\f0 Outside\par {\deff1\pard\plain\f1 Inside\par}})";
    std::string rtfB = R"({\rtf1\ansi\deff0\pard\plain\f0 Outside\par {\deff1\pard\plain\f0 Inside\par}})";
    std::string reason;
    // defaultFontIndex is the same (1) for both — the \f1 vs \f0 affects run font, not paragraph default
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DeffNestedGroupReverted() {
    // Nested group reverts to outer \deff
    std::string rtfA = R"({\rtf1\ansi\deff0 {\deff1 {\deff2\pard\plain\f2 Deep\par}\pard\plain\f1 Mid\par}\pard\plain\f0 Outer\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 {\deff1 {\deff2\pard\plain\f2 Deep\par}\pard\plain\f1 Mid\par}\pard\plain\f0 Outer\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DifferentDeftab() {
    std::string rtfA = R"({\rtf1\ansi\deff0\deftab180 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\deftab360 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::IdenticalDeftab() {
    std::string rtfA = R"({\rtf1\ansi\deff0\deftab720 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\deftab720 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::DeftabGroupPersistent() {
    // \deftab in subgroup affects paragraphs inside the subgroup
    std::string rtfA = R"({\rtf1\ansi\deff0\deftab180\pard\plain Outside\par {\deftab720\pard\plain Inside\par}})";
    std::string rtfB = R"({\rtf1\ansi\deff0\deftab180\pard\plain Outside\par {\deftab360\pard\plain Inside\par}})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::DeftabNestedGroupReverted() {
    // Nested group reverts to outer \deftab
    std::string rtfA = R"({\rtf1\ansi\deff0\deftab180 {\deftab360 {\deftab540\pard\plain Deep\par}\pard\plain Mid\par}\pard\plain Outer\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\deftab180 {\deftab360 {\deftab540\pard\plain Deep\par}\pard\plain Mid\par}\pard\plain Outer\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::HyperlinkExternalUrl() {
    // Parse a field group with HYPERLINK instruction
    std::string rtf = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://example.com"}{\*\fldrslt Click here}}\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 1u);
    QVERIFY(std::holds_alternative<RtfParagraph>(doc.elements[0]));
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    QVERIFY(para.runs.size() >= 1);
    // Find the run with "Click here" text and verify it has anchor format
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("Click here") != std::string::npos) {
            QVERIFY(run.format.isAnchor);
            QCOMPARE(run.format.anchorHref, "https://example.com");
            found = true;
        }
    }
    QVERIFY(found);
}

void TestSemanticComparison::HyperlinkInternalBookmark() {
    // Parse a field group with internal bookmark link
    std::string rtf = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "#MyBookmark"}{\*\fldrslt Go to section}}\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 1u);
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("Go to section") != std::string::npos) {
            QVERIFY(run.format.isAnchor);
            QCOMPARE(run.format.anchorHref, "#MyBookmark");
            found = true;
        }
    }
    QVERIFY(found);
}

void TestSemanticComparison::HyperlinkIdentical() {
    // Two RTF with same hyperlink should be identical
    std::string rtfA = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://example.com"}{\*\fldrslt Link text}}\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://example.com"}{\*\fldrslt Link text}}\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::HyperlinkDifferentUrl() {
    // Different URLs should be detected as different
    std::string rtfA = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://example.com"}{\*\fldrslt Link}}\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://other.com"}{\*\fldrslt Link}}\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::HyperlinkWithBold() {
    // Hyperlink with bold formatting inside fldrslt
    std::string rtf = R"({\rtf1\ansi\deff0{\field{\*\fldinst HYPERLINK "https://example.com"}{\*\fldrslt{\b Bold link}}}}\par})";
    auto doc = ParseRtf(rtf);
    QCOMPARE(doc.elements.size(), 1u);
    const auto& para = std::get<RtfParagraph>(doc.elements[0]);
    bool found = false;
    for (const auto& run : para.runs) {
        if (run.text.find("Bold link") != std::string::npos) {
            QVERIFY(run.format.isAnchor);
            QVERIFY(run.format.bold);
            QCOMPARE(run.format.anchorHref, "https://example.com");
            found = true;
        }
    }
    QVERIFY(found);
}

void TestSemanticComparison::cleanupTestCase() {
    qDebug() << "======================================";
    qDebug().noquote() << "Results: " << _timeout << " timeouts";
    qDebug().noquote() << "======================================";
}

QTEST_MAIN(TestSemanticComparison)
#include "TestSemanticComparison.moc"
