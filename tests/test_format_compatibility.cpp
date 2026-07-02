#include <RichTextEdit.h>
#include "RtfImport.h"
#include <QtTest>
#include <stdexcept>

class TestFormatCompatibility : public QObject {
    Q_OBJECT

private slots:
    void test_delphi_compatible_bold();
    void test_delphi_compatible_italic();
    void test_delphi_compatible_color();
    void test_delphi_compatible_empty();
    void test_delphi_compatible_plain();
    void test_delphi_compatible_rtf_header();
    void test_load_plain_text();
    void test_is_delphi_compatible_html();
};

void TestFormatCompatibility::test_delphi_compatible_bold() {
    std::string rtf = R"({\rtf1\ansi{\b Bold}{\b0 normal}})";
    QVERIFY(Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_delphi_compatible_italic() {
    std::string rtf = R"({\rtf1\ansi{\i Italic}{\i0 normal}})";
    QVERIFY(Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_delphi_compatible_color() {
    std::string rtf = R"({\rtf1\ansi{\colortbl;\red255\green0\blue0;}{\cf1 Red}})";
    QVERIFY(Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_delphi_compatible_empty() {
    std::string rtf;
    QVERIFY(!Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_delphi_compatible_plain() {
    std::string rtf = R"({\rtf1\ansi plain text})";
    QVERIFY(!Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_delphi_compatible_rtf_header() {
    std::string rtf = R"({\b Bold})";
    QVERIFY(Rte::isDelphiCompatible(rtf));
}

void TestFormatCompatibility::test_load_plain_text() {
    Rte::RichTextEdit editor;
    editor.load("Hello World", Rte::FormatMode::Rtf);
    QVERIFY(editor.toPlainText().contains("Hello World"));
}

void TestFormatCompatibility::test_is_delphi_compatible_html() {
    std::string html = "<b>Bold</b>";
    QVERIFY(!Rte::isDelphiCompatible(html));
}

QTEST_MAIN(TestFormatCompatibility)
#include "test_format_compatibility.moc"
