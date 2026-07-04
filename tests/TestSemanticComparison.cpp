#include <QtTest>
#include <future>
#include <chrono>
#include "RtfCompare.h"

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

    // RE 2.0 — Semantic identity (must NOT flag as different)
    void CbSemantic();

    // Images — must detect differences
    void DifferentImageCount();
    void DifferentImageFormat();
    void DifferentImageData();

    // Images — semantic identity (different dimensions, same data)
    void SemanticImageData();

    // Edge cases
    void EmptyDocs();
    void HeaderOnly();
    void UnknownTags();

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

void TestSemanticComparison::cleanupTestCase() {
    qDebug() << "======================================";
    qDebug().noquote() << "Results: " << _timeout << " timeouts";
    qDebug().noquote() << "======================================";
}

QTEST_MAIN(TestSemanticComparison)
#include "TestSemanticComparison.moc"
