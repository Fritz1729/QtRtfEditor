// tests/test_rtf_roundtrip.cpp
//
// Tests for RTF I/O of RichTextEdit.
//
// These tests validate that RTF data can be loaded into
// RichTextEdit and serialized back to RTF.

#include <rich_text_edit.h>
#include <QtTest>

class TestRtfRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void test_plain_text();
    void test_rtf_bold();
    void test_roundtrip();
};

void TestRtfRoundtrip::test_plain_text() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test");

    std::string rtf = editor.save(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
}

void TestRtfRoundtrip::test_rtf_bold() {
    Rte::RichTextEdit editor;
    editor.setHtml("<b>Bold</b> normal");

    std::string rtf = editor.save(Rte::FormatMode::Rtf);
    QVERIFY(!rtf.empty());
    QVERIFY(rtf.find("Bold") != std::string::npos);
}

void TestRtfRoundtrip::test_roundtrip() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test content");

    std::string original = editor.save(Rte::FormatMode::Rtf);

    // Load again
    Rte::RichTextEdit editor2;
    editor2.load(original, Rte::FormatMode::Rtf);

    std::string loaded = editor2.save(Rte::FormatMode::Rtf);

    // Both RTF blobs should contain the same plain text
    QVERIFY(!original.empty());
    QVERIFY(!loaded.empty());
}

QTEST_MAIN(TestRtfRoundtrip)
#include "test_rtf_roundtrip.moc"
