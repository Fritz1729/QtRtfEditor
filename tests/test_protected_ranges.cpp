#include <RichTextEdit.h>
#include <QtTest>

class TestProtectedRanges : public QObject {
    Q_OBJECT

    friend class RichTextEdit;

private slots:
    void initTestCase();
    void test_basic_protection();
    void test_multiple_ranges();
    void test_overlapping_ranges();
    void test_protection_policy_none();
    void test_protection_policy_block();
    void test_protection_policy_warn();
    void test_handler();
    void test_all_protection();
    void test_is_protected();
    void test_clear_protection();
    void test_key_press_backspace();
};

void TestProtectedRanges::initTestCase() {}

void TestProtectedRanges::test_basic_protection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");

    editor.setProtection(0, 5, "test-type", "test-target");

    QVERIFY(editor.isProtected(0));
    QVERIFY(editor.isProtected(2));
    QVERIFY(editor.isProtected(4));
    QVERIFY(!editor.isProtected(5));
    QVERIFY(!editor.isProtected(10));
}

void TestProtectedRanges::test_multiple_ranges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("AAA BBB CCC");

    editor.setProtection(0, 3, "type1", "target1");
    editor.setProtection(5, 8, "type2", "target2");

    QVERIFY(editor.isProtected(1));
    QVERIFY(editor.isProtected(6));
    QVERIFY(!editor.isProtected(3));
    QVERIFY(!editor.isProtected(4));
    QVERIFY(!editor.isProtected(8));
}

void TestProtectedRanges::test_overlapping_ranges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABCDEF");

    editor.setProtection(0, 4, "type1", "target1");
    editor.setProtection(2, 6, "type2", "target2");

    std::vector<Rte::ProtectedRangeInfo> all = editor.allProtection();
    QCOMPARE(all.size(), static_cast<std::size_t>(2));

    QVERIFY(editor.isProtected(0));
    QVERIFY(editor.isProtected(1));
    QVERIFY(editor.isProtected(2));
    QVERIFY(editor.isProtected(3));
    QVERIFY(editor.isProtected(4));
    QVERIFY(editor.isProtected(5));
}

void TestProtectedRanges::test_protection_policy_none() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::None);
    editor.setProtection(0, 5, "test", "target");

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);
}

void TestProtectedRanges::test_protection_policy_block() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Block);
    editor.setProtection(0, 5, "test", "target");

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0, QTextCursor::KeepAnchor);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);

    QCOMPARE(editor.toPlainText(), QString("Hello World"));
}

void TestProtectedRanges::test_protection_policy_warn() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.setProtection(0, 5, "test", "target");

    bool handlerCalled = false;
    editor.setProtectionViolationHandler(
        [&](const Rte::ProtectedRangeInfo &,
            const QTextCursor &) -> bool {
            handlerCalled = true;
            return false;
        });

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0, QTextCursor::KeepAnchor);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);

    QVERIFY(handlerCalled);
    QCOMPARE(editor.toPlainText(), QString("Hello World"));
}

void TestProtectedRanges::test_handler() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.setProtection(0, 5, "test", "target");

    std::string receivedType;
    std::string receivedTarget;
    bool handlerCalled = false;
    editor.setProtectionViolationHandler(
        [&](const Rte::ProtectedRangeInfo &info,
            const QTextCursor &) -> bool {
            handlerCalled = true;
            receivedType = info.type;
            receivedTarget = info.target;
            return true;
        });

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0, QTextCursor::KeepAnchor);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);

    QVERIFY(handlerCalled);
    QCOMPARE(receivedType, std::string("test"));
    QCOMPARE(receivedTarget, std::string("target"));
    QCOMPARE(editor.toPlainText().size(),
             QString("Hello World").size() - 5);
}

void TestProtectedRanges::test_all_protection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC DEF GHI");

    editor.setProtection(0, 3, "type1", "target1");
    editor.setProtection(5, 8, "type2", "target2");

    std::vector<Rte::ProtectedRangeInfo> all = editor.allProtection();
    QCOMPARE(all.size(), static_cast<std::size_t>(2));
    QCOMPARE(all[0].type, std::string("type1"));
    QCOMPARE(all[0].target, std::string("target1"));
    QCOMPARE(all[1].type, std::string("type2"));
    QCOMPARE(all[1].target, std::string("target2"));
}

void TestProtectedRanges::test_is_protected() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test");

    QVERIFY(!editor.isProtected(0));
    QVERIFY(!editor.isProtected(1));

    editor.setProtection(1, 3, "test", "target");
    QVERIFY(!editor.isProtected(0));
    QVERIFY(editor.isProtected(1));
    QVERIFY(editor.isProtected(2));
    QVERIFY(!editor.isProtected(3));
}

void TestProtectedRanges::test_clear_protection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC");

    editor.setProtection(0, 3, "test", "target");
    QCOMPARE(editor.allProtection().size(), static_cast<std::size_t>(1));

    editor.clearProtection();
    QCOMPARE(editor.allProtection().size(), static_cast<std::size_t>(0));
    QVERIFY(!editor.isProtected(0));
}

void TestProtectedRanges::test_key_press_backspace() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    editor.keyPressEvent(&event);
}

QTEST_MAIN(TestProtectedRanges)
#include "test_protected_ranges.moc"
