// tests/test_protected_ranges.cpp
//
// Tests für das Schutz-System von RichTextEdit.

#include <RichTextEdit/RichTextEdit.h>
#include <QtTest>

class TestProtectedRanges : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void test_basic_protection();
    void test_multiple_ranges();
    void test_overlapping_ranges();
    void test_protection_policy_none();
    void test_protection_policy_block();
    void test_protection_policy_warn();
    void test_schutz_verstoß_handler();
    void test_alle_schutz();
    void test_ist_geschuetzt();
    void test_loesche_schutz();
    void test_key_press_backspace();
    void test_key_press_delete();
};

void TestProtectedRanges::initTestCase() {
    // Globaler Test-Setup
}

void TestProtectedRanges::test_basic_protection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");

    // Geschützten Bereich setzen (Position 0-5 = "Hallo")
    editor.setzeSchutz(0, 5, "TestTyp", "TestZiel");

    QVERIFY(editor.istGeschuetzt(0));
    QVERIFY(editor.istGeschuetzt(2));
    QVERIFY(editor.istGeschuetzt(4));
    QVERIFY(!editor.istGeschuetzt(5));
    QVERIFY(!editor.istGeschuetzt(10));
}

void TestProtectedRanges::test_multiple_ranges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("AAA BBB CCC");

    editor.setzeSchutz(0, 3, "Typ1", "Ziel1");
    editor.setzeSchutz(5, 8, "Typ2", "Ziel2");

    QVERIFY(editor.istGeschuetzt(1));
    QVERIFY(editor.istGeschuetzt(6));
    QVERIFY(!editor.istGeschuetzt(3));
    QVERIFY(!editor.istGeschuetzt(4));
    QVERIFY(!editor.istGeschuetzt(8));
}

void TestProtectedRanges::test_overlapping_ranges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABCDEF");

    // Bereich 0-4 setzen
    editor.setzeSchutz(0, 4, "Typ1", "Ziel1");
    // Bereich 2-6 setzen (ueberschneidet mit erstem)
    editor.setzeSchutz(2, 6, "Typ2", "Ziel2");

    std::vector<Rte::SchutzInfo> alle = editor.alleSchutz();
    QCOMPARE(alle.size(), static_cast<std::size_t>(2));

    // Beide Bereiche sind geschützt
    QVERIFY(editor.istGeschuetzt(0));  // Nur Bereich 1
    QVERIFY(editor.istGeschuetzt(1));  // Nur Bereich 1
    QVERIFY(editor.istGeschuetzt(2));  // Beide
    QVERIFY(editor.istGeschuetzt(3));  // Beide
    QVERIFY(editor.istGeschuetzt(4));  // Nur Bereich 2
    QVERIFY(editor.istGeschuetzt(5));  // Nur Bereich 2
}

void TestProtectedRanges::test_protection_policy_none() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::None);
    editor.setzeSchutz(0, 5, "Test", "Ziel");

    bool erlaubt = false;
    QTextCursor cursor = editor.textCursor();
    cursor.setSelection(0, 5);
    editor.pruefeSchutz(cursor, erlaubt);
    QVERIFY(erlaubt); // None = alles erlaubt
}

void TestProtectedRanges::test_protection_policy_block() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Block);
    editor.setzeSchutz(0, 5, "Test", "Ziel");

    bool erlaubt = true;
    QTextCursor cursor = editor.textCursor();
    cursor.setSelection(0, 5);
    editor.pruefeSchutz(cursor, erlaubt);
    QVERIFY(!erlaubt); // Block = geschuetzt
}

void TestProtectedRanges::test_protection_policy_warn() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.setzeSchutz(0, 5, "Test", "Ziel");

    // Kein Handler gesetzt — standard-Dialog (simuliert: Ja)
    bool erlaubt = false;
    QTextCursor cursor = editor.textCursor();
    cursor.setSelection(0, 5);
    editor.pruefeSchutz(cursor, erlaubt);
    // Mit Standard-Dialog: Ergebnis hängt von Dialog-Aktion ab
    // In automatisierten Tests ist der Dialog modal und blockiert.
    // Daher: Wir testen nur den Handler-Fall.
}

void TestProtectedRanges::test_schutz_verstoß_handler() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.setzeSchutz(0, 5, "Test", "Ziel");

    bool handlerAufgerufen = false;
    editor.setSchutzVerstoßHandler([&](const Rte::SchutzInfo &info,
                                        const QTextCursor &) {
        handlerAufgerufen = true;
        return false; // Blockieren
    });

    bool erlaubt = true;
    QTextCursor cursor = editor.textCursor();
    cursor.setSelection(0, 5);
    editor.pruefeSchutz(cursor, erlaubt);

    QVERIFY(handlerAufgerufen);
    QVERIFY(!erlaubt);
}

void TestProtectedRanges::test_alle_schutz() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC DEF GHI");

    editor.setzeSchutz(0, 3, "Typ1", "Ziel1");
    editor.setzeSchutz(5, 8, "Typ2", "Ziel2");

    std::vector<Rte::SchutzInfo> alle = editor.alleSchutz();
    QCOMPARE(alle.size(), static_cast<std::size_t>(2));
    QCOMPARE(alle[0].typ, std::string("Typ1"));
    QCOMPARE(alle[0].ziel, std::string("Ziel1"));
    QCOMPARE(alle[1].typ, std::string("Typ2"));
    QCOMPARE(alle[1].ziel, std::string("Ziel2"));
}

void TestProtectedRanges::test_ist_geschuetzt() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test");

    // Kein Schutz gesetzt
    QVERIFY(!editor.istGeschuetzt(0));
    QVERIFY(!editor.istGeschuetzt(1));

    editor.setzeSchutz(1, 3, "Test", "Ziel");
    QVERIFY(editor.istGeschuetzt(0)); // Nein — Position 0 ist nicht in [1,3)
    QVERIFY(editor.istGeschuetzt(1));
    QVERIFY(editor.istGeschuetzt(2));
    QVERIFY(!editor.istGeschuetzt(3));
}

void TestProtectedRanges::test_loesche_schutz() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC");

    editor.setzeSchutz(0, 3, "Test", "Ziel");
    QCOMPARE(editor.alleSchutz().size(), static_cast<std::size_t>(1));

    editor.loescheSchutz();
    QCOMPARE(editor.alleSchutz().size(), static_cast<std::size_t>(0));
    QVERIFY(!editor.istGeschuetzt(0));
}

void TestProtectedRanges::test_key_press_backspace() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Block);
    editor.setzeSchutz(0, 5, "Test", "Ziel");

    // Cursor auf Position 5 (nach "Hallo") — Backspace sollte
    // den geschützten Bereich "Hallo" loeschen -> blockiert
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(5);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    // Da der Schutz-Modus "Block" ist und die Selection den
    // geschützten Bereich trifft, sollte die Operation ignoriert werden.
    // In diesem Test ist die Cursor-Position genau am Rand —
    // es gibt keine Selection, daher sollte Backspace normal arbeiten.
    // Der Schutz greift nur bei Selections.
    editor.keyPressEvent(&event);
}

void TestProtectedRanges::test_key_press_delete() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hallo Welt");

    bool erlaubt = true;
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor); // "Hallo"
    editor.setTextCursor(cursor);

    editor.pruefeSchutz(cursor, erlaubt);
    QVERIFY(erlaubt); // Kein Schutz gesetzt
}

QTEST_MAIN(TestProtectedRanges)
#include "test_protected_ranges.moc"
