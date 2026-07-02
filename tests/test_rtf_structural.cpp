#include <QtTest>
#include <stdexcept>
#include <future>
#include <chrono>
#include "RtfCompare.h"

using namespace Rte;

// ── Timeout guard for parser-heavy operations ──────────────────
// ParseRtf() may hang on certain control words (\colortbl, etc.).
// These helpers run the operation in a detached background thread
// and time out after 3 seconds.

struct CompareResult {
    RtfCompareResult result = RtfCompareResult::StructuralDiff;
    std::string reason;
    bool ok = false;
};

static CompareResult SafeCompareRtf(const std::string &rtfA, const std::string &rtfB) {
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

static CompareResult CompareWithTimeout(const std::string &rtfA, const std::string &rtfB, int sec) {
    std::promise<CompareResult> promise;
    std::future<CompareResult> future = promise.get_future();

    std::thread t([&]() {
        CompareResult r = SafeCompareRtf(rtfA, rtfB);
        promise.set_value(r);
    });
    t.detach();

    auto status = future.wait_for(std::chrono::seconds(sec));
    if (status != std::future_status::ready) {
        return CompareResult{};
    }
    return future.get();
}

// ── TestSemanticComparison: atomic unit tests for CompareRtf() ──

class TestSemanticComparison : public QObject {
    Q_OBJECT

private slots:
    // True positives — must detect differences
    void identical_rtf();
    void different_text();
    void different_formatting();
    void different_paragraph_count();

    // False positive guards — must NOT flag as different
    void semantic_color();
    void semantic_font();

    // False negative guards — must flag as different
    void different_alignment();
    void different_indent();
    void different_superscript();

    // Edge cases
    void empty_docs();
    void header_only();
    void unknown_tags();

    void cleanupTestCase();

private:
    int _passed = 0;
    int _failed = 0;
    int _timeout = 0;
};

void TestSemanticComparison::identical_rtf() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
    QVERIFY(reason.empty());
}

void TestSemanticComparison::different_text() {
    std::string rtfA = R"({\rtf1\ansi\deff0 Hello\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 World\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::different_formatting() {
    std::string rtfA = R"({\rtf1\ansi\deff0{\b Bold}\b0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Bold\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::different_paragraph_count() {
    std::string rtfA = R"({\rtf1\ansi\deff0 One\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 One\par Two\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
    QVERIFY(!reason.empty());
}

void TestSemanticComparison::semantic_color() {
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

void TestSemanticComparison::semantic_font() {
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

void TestSemanticComparison::different_alignment() {
    std::string rtfA = R"({\rtf1\ansi\deff0\ql Left\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\qc Left\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::different_indent() {
    std::string rtfA = R"({\rtf1\ansi\deff0\li500 Text\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0\li200 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::different_superscript() {
    std::string rtfA = R"({\rtf1\ansi\deff0 H\super 2\super0\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 H 2\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::StructuralDiff);
}

void TestSemanticComparison::empty_docs() {
    std::string rtfA = R"({\rtf1\ansi\deff0})";
    std::string rtfB = R"({\rtf1\ansi\deff0})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::header_only() {
    std::string rtfA = R"({\rtf1\ansi\deff0})";
    std::string rtfB = R"({\rtf1\ansi\deff0\rtf1\ansi})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::Identical);
}

void TestSemanticComparison::unknown_tags() {
    // \xyz is not a recognized control word and not \u (unicode escape)
    std::string rtfA = R"({\rtf1\ansi\deff0 Text\xyz\par})";
    std::string rtfB = R"({\rtf1\ansi\deff0 Text\par})";
    std::string reason;
    QCOMPARE(CompareRtf(rtfA, rtfB, reason), RtfCompareResult::UnknownTag);
}

void TestSemanticComparison::cleanupTestCase() {
    qDebug() << "======================================";
    qDebug().noquote() << "Results: " << _passed << " passed, " << _failed
                       << " failed, " << _timeout << " timeouts";
    qDebug().noquote() << "======================================";
}

QTEST_MAIN(TestSemanticComparison)
#include "test_rtf_structural.moc"
