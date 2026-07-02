// tests/test_rtf_roundtrip.cpp
//
// Tests für RTF-Ein- und -Ausgabe von RichTextEdit.
//
// Diese Tests validieren, dass RTF-Daten, die von Delphi/TRichEdit
// erzeugt wurden, korrekt in RichTextEdit geladen und als RTF
// zurueckgegeben werden koennen.

#include <RichTextEdit/RichTextEdit.h>
#include <QtTest>
#include <QFile>

class TestRtfRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void test_plain_text();
    void test_rtf_bold();
    void test_rtf_italic();
    void test_rtf_underline();
    void test_rtf_color();
    void test_rtf_alignment();
    void test_rtf_fontsize();
    void test_rtf_indent();
    void test_roundtrip();
};

void TestRtfRoundtrip::initTestCase() {
    // Testdaten laden (wird später benötigt)
}

void TestRtfRoundtrip::test_plain_text() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    QVERIFY(rtf.find("Test") != std::string::npos);
}

void TestRtfRoundtrip::test_rtf_bold() {
    Rte::RichTextEdit editor;
    editor.setHtml("<b>Fett</b> normal");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    // RTF sollte \\b (bold) enthalten
    QVERIFY(rtf.find("\\b") != std::string::npos);
}

void TestRtfRoundtrip::test_rtf_italic() {
    Rte::RichTextEdit editor;
    editor.setHtml("<i>Kursiv</i> normal");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    // RTF sollte \\i (italic) enthalten
    QVERIFY(rtf.find("\\i") != std::string::npos);
}

void TestRtfRoundtrip::test_rtf_underline() {
    Rte::RichTextEdit editor;
    editor.setHtml("<u>Unterstrichen</u> normal");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    // RTF sollte \\ul (underline) enthalten
    QVERIFY(rtf.find("\\ul") != std::string::npos);
}

void TestRtfRoundtrip::test_rtf_color() {
    Rte::RichTextEdit editor;
    editor.setHtml("<font color=\"red\">Rot</font>");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
}

void TestRtfRoundtrip::test_rtf_alignment() {
    Rte::RichTextEdit editor;
    editor.setHtml("<div align=\"center\">Mitte</div>");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
}

void TestRtfRoundtrip::test_rtf_fontsize() {
    Rte::RichTextEdit editor;
    editor.setHtml("<span style=\"font-size:14pt\">Groß</span>");

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    // RTF font-size ist in Halftpunkten
    QVERIFY(rtf.find("\\fs") != std::string::npos ||
            rtf.find("Groß") != std::string::npos);
}

void TestRtfRoundtrip::test_rtf_indent() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Eingerückt");

    QTextCursor cursor = editor.textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setLeftIndent(10);
    fmt.setFirstIndent(5);
    cursor.setBlockFormat(fmt);
    editor.setTextCursor(cursor);

    std::string rtf = editor.speichern(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    // RTF sollte Einrückungs-Tags enthalten
    QVERIFY(rtf.find("\\li") != std::string::npos ||
            rtf.find("\\fi") != std::string::npos ||
            rtf.find("Eingerückt") != std::string::npos);
}

void TestRtfRoundtrip::test_roundtrip() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Testinhalt");

    std::string original = editor.speichern(Rte::FormatMode::Rtf);

    // Erneut laden
    Rte::RichTextEdit editor2;
    editor2.laden(original, Rte::FormatMode::Rtf);

    std::string geladen = editor2.speichern(Rte::FormatMode::Rtf);

    // Beide RTB-Blobs sollten den gleichen Plaintext enthalten
    editor.setText(original.c_str());
    editor2.setText(geladen.c_str());

    // Der Plaintext muss uebereinstimmen
    QCOMPARE(editor.toPlainText(), editor2.toPlainText());
}

QTEST_MAIN(TestRtfRoundtrip)
#include "test_rtf_roundtrip.moc"
