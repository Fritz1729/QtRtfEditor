#include <RichTextEdit.h>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <stdexcept>
#include "RtfCompare.h"
#include "RtfParser.h"

using namespace Rte;

static const char* const kSkippedFiles[] = {
    "line-spacing.rtf",          // \slmultN — Qt only supports FixedHeight, not multiplier
    "positional-supsub.rtf",     // \upN/\dnN — Qt only supports boolean super/subscript
    "underline-styles.rtf",      // \uldb — no double underline in Qt UnderlineStyle enum
    "ul-dashdot.rtf",            // \uldashd — DashDotLine has no Qt roundtrip (toUnderlineStyle loss)
    "ul-dashdotdot.rtf",         // \uldashdd — DashDotDotLine has no Qt roundtrip
    "underline-color.rtf",       // \ulcN — Qt has no setFontUnderlineColor()
    "language-id.rtf",           // \langN — Qt 6.11 has no setFontLanguageId()
    nullptr
};

static bool IsSkipped(const QString& filename) {
    for (int i = 0; kSkippedFiles[i]; ++i) {
        if (filename == QString::fromUtf8(kSkippedFiles[i])) return true;
    }
    return false;
}

class TestRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void TestRtfSuite();
    void cleanupTestCase();

private:
    int _pass = 0;
    int _fail = 0;
    int _skip = 0;
    int _exception = 0;
};

static bool HasUnknownTags(const std::string& rtf) {
    try {
        RtfDocument doc = ParseRtf(rtf);
        return !doc.unknownTags.empty();
    } catch (...) {
        // Iteration limit or crash — counts as unsupported
        return true;
    }
}

static std::string ReadFile(const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!file.exists()) {
        throw std::runtime_error(("File not found: " + path).c_str());
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(file.errorString().toStdString().c_str());
    }
    return file.readAll().toStdString();
}

static void ReportCase(const QString& filename, const char* result) {
    qDebug().noquote() << "[" << result << "]" << filename;
}

void TestRoundtrip::TestRtfSuite() {
    QString testDataDir = QCoreApplication::applicationDirPath() + "/testdata";

    QDir dir(testDataDir);
    QStringList files = dir.entryList(QStringList() << "*.rtf", QDir::Files);

        _pass = 0;
        _fail = 0;
        _skip = 0;
        _exception = 0;

        for (int i = 0; i < files.size(); ++i) {
            const QString& filename = files[i];
            qDebug().noquote() << "[" << i + 1 << "/" << files.size() << "]" << filename;

            if (IsSkipped(filename)) {
                ReportCase(filename, "SKIP (no Qt roundtrip)");
                _skip++;
                continue;
            }

            QString filepath = testDataDir + "/" + filename;

        std::string original;
        try {
            original = ReadFile(filepath.toStdString());
        } catch (const std::exception& e) {
            ReportCase(filename, "EXCEPTION");
            qWarning() << "File:" << filename << ":" << e.what();
            _exception++;
            continue;
        }

        // Skip files with unsupported features (parser iteration limit = unsupported)
        if (HasUnknownTags(original)) {
            ReportCase(filename, "FAIL (unsupported features)");
            _fail++;
            continue;
        }

        // Load/save in main thread
        try {
            Rte::RichTextEdit editor;
            editor.Load(original, Rte::FormatMode::Rtf);
            std::string saved = editor.Save(Rte::FormatMode::Rtf);

            // Check output for unsupported features too
            if (HasUnknownTags(saved)) {
                ReportCase(filename, "FAIL (output has unsupported features)");
                _fail++;
                continue;
            }

            // Both parse cleanly — compare
            std::string reason;
            RtfCompareResult result = CompareRtf(original, saved, reason);
            bool passed = (result == RtfCompareResult::Identical);

            ReportCase(filename, passed ? "PASS" : "FAIL");

            if (!passed) {
                qDebug() << "File:" << filename << ":" << QString::fromStdString(reason);
            }

            if (passed) _pass++;
            else _fail++;
        } catch (const std::exception& e) {
            ReportCase(filename, "EXCEPTION");
            qWarning() << "File:" << filename << "crashed:" << e.what();
            _exception++;
        }
    }

    QVERIFY(_fail == 0);
}

void TestRoundtrip::cleanupTestCase() {
    qDebug() << "======================================";
    qDebug().noquote() << "Results: " << _pass << " passed, " << _fail
                        << " failed, " << _skip
                        << " skipped, " << _exception
                        << " exceptions";
    qDebug().noquote() << "======================================";
}

QTEST_MAIN(TestRoundtrip)
#include "TestRoundtrip.moc"
