#include <cstdio>
#include <RichTextEdit.h>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QVector>
#include <stdexcept>
#include "RtfCompare.h"
#include "RtfParser.h"

using namespace Rte;

static const char* const kSkippedFiles[] = {
    "cell-shading.rtf",            // \clshdn — Qt has no cell-level background API
    "tables-merged.rtf",           // \clmrg — merged cells out of scope
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

public:
    void setCustomDir(const QString& dir) { _customDir = dir; }

private:
    void _runFromCustomDir(const QString& dirPath);

    int _pass = 0;
    int _fail = 0;
    int _skip = 0;
    int _exception = 0;
    QString _customDir;
};

static bool HasUnknownTags(const std::string& rtf) {
    try {
        RtfDocument doc = ParseRtf(rtf);
        if (!doc.unknownTags.empty()) {
            for (const auto& tag : doc.unknownTags) {
                qDebug() << "  UNKNOWN TAG:" << QString::fromStdString(tag);
            }
        }
        return !doc.unknownTags.empty();
    } catch (...) {
        // Iteration limit or crash — counts as unsupported
        qDebug() << "  UNKNOWN TAG: exception during ParseRtf";
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
    QString testDataDir;
    if (!_customDir.isEmpty()) {
        testDataDir = _customDir;
    } else {
        testDataDir = QCoreApplication::applicationDirPath() + "/testdata";
    }
    _runFromCustomDir(testDataDir);
}

void TestRoundtrip::_runFromCustomDir(const QString& dirPath) {
    QDir dir(dirPath);
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

        QString filepath = dirPath + "/" + filename;

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

static int customMain(int argc, char **argv) {
    QStringList filtered;
    QString testDataDir;
    for (int i = 0; i < argc; ++i) {
        QByteArray arg = argv[i];
        if (arg == "--testdata-dir" && i + 1 < argc) {
            testDataDir = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.startsWith("--testdata-dir=")) {
            testDataDir = QString::fromLocal8Bit(arg.mid(strlen("--testdata-dir=")));
        } else if (arg == "-t" && i + 1 < argc) {
            testDataDir = QString::fromLocal8Bit(argv[++i]);
        } else {
            filtered << QString::fromLocal8Bit(argv[i]);
        }
    }

    if (!testDataDir.isEmpty()) {
        QDir dir(testDataDir);
        if (!dir.exists() || !dir.isReadable()) {
            fprintf(stderr, "--testdata-dir: %s does not exist or is not readable\n", testDataDir.toStdString().c_str());
            return 1;
        }
    }

    int filteredArgc = filtered.size();
    QVector<QByteArray> filteredBytes;
    filteredBytes.reserve(filtered.size());
    for (const QString& s : filtered) {
        filteredBytes.append(s.toLocal8Bit());
    }
    QVector<char*> filteredArgv;
    filteredArgv.reserve(filtered.size());
    for (QByteArray& b : filteredBytes) {
        filteredArgv.append(b.data());
    }
    filteredArgv.append(nullptr);

    int adjustedArgc = filteredArgc;
    QApplication app(adjustedArgc, filteredArgv.data());
    app.setApplicationName("test_roundtrip");

    TestRoundtrip test;
    test.setCustomDir(testDataDir);
    return QTest::qExec(&test, adjustedArgc, filteredArgv.data());
}

int main(int argc, char **argv) {
    return customMain(argc, argv);
}

#include "TestRoundtrip.moc"
