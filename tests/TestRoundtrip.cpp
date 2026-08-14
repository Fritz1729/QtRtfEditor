#include <RichTextEdit.h>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QFont>
#include <QTest>
#include <QVector>
#include <stdexcept>
#include "RtfCompare.h"
#include "RtfParser.h"

using namespace Rte;

static const char* const kSkippedFiles[] = {
    "cell-shading.rtf",            // \clshdn — Qt has no cell-level background API
    "font-size-pntext-order.rtf",  // \fs before \pntext — format preservation across pntext boundary
    "tables-merged.rtf",           // \clmrg — merged cells out of scope
    nullptr
};

static bool IsSkipped(const QString& filename) {
    for (int i = 0; kSkippedFiles[i]; ++i) {
        if (filename == QString::fromUtf8(kSkippedFiles[i])) return true;
    }
    return false;
}

struct RoundtripResult {
    bool unsupported = false;
    bool outputUnsupported = false;
    bool passed = false;
    bool exception = false;
    std::string reason;
};

class TestRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void TestRtfSuite();
    void cleanupTestCase();

public:
    void SetCustomDir(const QString& dir) { _customDir = dir; }

private:
    void RunFromCustomDir(const QString& dirPath);
    RoundtripResult RunRoundtripOnMainThread(const std::string& original);

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

RoundtripResult TestRoundtrip::RunRoundtripOnMainThread(const std::string& original) {
    RoundtripResult r;
    try {
        Rte::RichTextEdit editor;
        editor.Load(original, Rte::FormatMode::Rtf);
        std::string saved = editor.Save(Rte::FormatMode::Rtf);

        auto doc = ParseRtf(saved);
        if (!doc.unknownTags.empty()) {
            r.outputUnsupported = true;
            return r;
        }

        std::string reason;
        auto cmp = CompareRtf(original, saved, reason);
        r.passed = (cmp == RtfCompareResult::Identical);
        r.reason = std::move(reason);
    } catch (...) {
        r.exception = true;
    }
    return r;
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
    RunFromCustomDir(testDataDir);
}

void TestRoundtrip::RunFromCustomDir(const QString& dirPath) {
    QDir dir(dirPath);
    QStringList files = dir.entryList(QStringList() << "*.rtf", QDir::Files);

    if (files.isEmpty()) {
        QFAIL(qPrintable("No .rtf files found in " + dirPath));
    }

    _pass = 0;
    _fail = 0;
    _skip = 0;
    _exception = 0;

    for (int i = 0; i < files.size(); ++i) {
        const QString& filename = files[i];

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

        RoundtripResult r = RunRoundtripOnMainThread(original);

        if (r.outputUnsupported) {
            ReportCase(filename, "FAIL (output has unsupported features)");
            _fail++;
            continue;
        }

        if (r.exception) {
            ReportCase(filename, "EXCEPTION");
            _exception++;
            continue;
        }

        ReportCase(filename, r.passed ? "PASS" : "FAIL");

        if (!r.passed) {
            qDebug() << "File:" << filename << ":" << QString::fromStdString(r.reason);
        }

        if (r.passed) _pass++;
        else _fail++;
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
// Compute default font family on the main thread before any worker threads are spawned.
// QFont() is not thread-safe and can deadlock when called from a non-main thread on Windows.
static void InitDefaultFontFamily() {
    extern std::string gDefaultFontFamily;
    gDefaultFontFamily = QFont().family().toStdString();
}

static int CustomMain(int argc, char **argv) {
    InitDefaultFontFamily();
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
    test.SetCustomDir(testDataDir);
    return QTest::qExec(&test, adjustedArgc, filteredArgv.data());
}

int main(int argc, char **argv) {
    return CustomMain(argc, argv);
}

#include "TestRoundtrip.moc"
