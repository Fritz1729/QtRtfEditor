// tests/test_format_compatibility.cpp
//
// Tests für Delphi/TRichEdit-kompatible RTF-Erkennung.
//
// Diese Tests validieren, dass die Funktion
// Rte::istDelphiKompatibel() TRichEdit-generierte RTF-Strukturen
// korrekt erkennt.

#include <RichTextEdit/RichTextEdit.h>
#include "rtf_import.h"
#include <QtTest>
#include <stdexcept>

class TestFormatCompatibility : public QObject {
    Q_OBJECT

private slots:
    void test_delphi_kompatibel_bold();
    void test_delphi_kompatibel_italic();
    void test_delphi_kompatibel_color();
    void test_delphi_kompatibel_empty();
    void test_delphi_kompatibel_plain();
    void test_delphi_kompatibel_rtf_header();
    void test_laden_leer();
    void test_laden_plain_text();
    void test_ist_delphi_kompatibel_html();
};

void TestFormatCompatibility::test_delphi_kompatibel_bold() {
    std::string rtf = R"({\rtf1\ansi{\b Fett}{\b0 normal}})";
    QVERIFY(Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_delphi_kompatibel_italic() {
    std::string rtf = R"({\rtf1\ansi{\i Kursiv}{\i0 normal}})";
    QVERIFY(Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_delphi_kompatibel_color() {
    std::string rtf = R"({\rtf1\ansi{\colortbl;\red255\green0\blue0;}{\cf1 Rot}})";
    QVERIFY(Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_delphi_kompatibel_empty() {
    std::string rtf;
    QVERIFY(!Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_delphi_kompatibel_plain() {
    std::string rtf = R"({\rtf1\ansi plain text})";
    QVERIFY(!Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_delphi_kompatibel_rtf_header() {
    // Ohne RTF-Header (nur Body)
    std::string rtf = R"({\b Fett})";
    QVERIFY(Rte::istDelphiKompatibel(rtf));
}

void TestFormatCompatibility::test_laden_leer() {
    Rte::RichTextEdit editor;
    QVERIFY_THROWS_EXCEPTION(std::runtime_error,
        editor.laden("", Rte::FormatMode::Rtf));
}

void TestFormatCompatibility::test_laden_plain_text() {
    Rte::RichTextEdit editor;
    // Reiner Text ohne RTF-Header — sollte einen minimalen
    // Header ergaenzt bekommen
    editor.laden("Hallo Welt", Rte::FormatMode::Rtf);
    QCOMPARE(editor.toPlainText(), QString("Hallo Welt"));
}

void TestFormatCompatibility::test_ist_delphi_kompatibel_html() {
    // HTML ist per Definition nicht Delphi-RTF-kompatibel
    std::string html = "<b>Fett</b>";
    QVERIFY(!Rte::istDelphiKompatibel(html));
}

QTEST_MAIN(TestFormatCompatibility)
#include "test_format_compatibility.moc"
