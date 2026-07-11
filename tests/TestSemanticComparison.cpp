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

    // Edge cases
    void EmptyDocs();
    void HeaderOnly();
    void UnknownTags();

    // Whitespace preservation and underline control words
    void WhitespaceAfterToggleOff();
    void UlNone();
    void SemanticUlSynonyms();

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

void TestSemanticComparison::cleanupTestCase() {
    qDebug() << "======================================";
    qDebug().noquote() << "Results: " << _timeout << " timeouts";
    qDebug().noquote() << "======================================";
}

QTEST_MAIN(TestSemanticComparison)
#include "TestSemanticComparison.moc"
