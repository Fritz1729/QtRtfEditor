#include <RichTextEdit.h>
#include <QtTest>

class TestProtectedRanges : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void TestBasicProtection();
    void TestMultipleRanges();
    void TestOverlappingRanges();
    void TestProtectionPolicyNone();
    void TestProtectionPolicyBlock();
    void TestProtectionPolicyWarn();
    void TestHandler();
    void TestAllProtection();
    void TestIsProtected();
    void TestClearProtection();
    void TestKeyPressBackspace();
};

void TestProtectedRanges::initTestCase() {}

void TestProtectedRanges::TestBasicProtection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");

    editor.SetProtection(0, 5, "test-type", "test-target");

    QVERIFY(editor.IsProtected(0));
    QVERIFY(editor.IsProtected(2));
    QVERIFY(editor.IsProtected(4));
    QVERIFY(!editor.IsProtected(5));
    QVERIFY(!editor.IsProtected(10));
}

void TestProtectedRanges::TestMultipleRanges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("AAA BBB CCC");

    editor.SetProtection(0, 3, "type1", "target1");
    editor.SetProtection(5, 8, "type2", "target2");

    QVERIFY(editor.IsProtected(1));
    QVERIFY(editor.IsProtected(6));
    QVERIFY(!editor.IsProtected(3));
    QVERIFY(!editor.IsProtected(4));
    QVERIFY(!editor.IsProtected(8));
}

void TestProtectedRanges::TestOverlappingRanges() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABCDEF");

    editor.SetProtection(0, 4, "type1", "target1");
    editor.SetProtection(2, 6, "type2", "target2");

    std::vector<Rte::ProtectedRangeInfo> all = editor.AllProtection();
    QCOMPARE(all.size(), static_cast<std::size_t>(2));

    QVERIFY(editor.IsProtected(0));
    QVERIFY(editor.IsProtected(1));
    QVERIFY(editor.IsProtected(2));
    QVERIFY(editor.IsProtected(3));
    QVERIFY(editor.IsProtected(4));
    QVERIFY(editor.IsProtected(5));
}

void TestProtectedRanges::TestProtectionPolicyNone() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtectionPolicy(Rte::ProtectionPolicy::None);
    editor.SetProtection(0, 5, "test", "target");

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);
}

void TestProtectedRanges::TestProtectionPolicyBlock() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtectionPolicy(Rte::ProtectionPolicy::Block);
    editor.SetProtection(0, 5, "test", "target");

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0, QTextCursor::KeepAnchor);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);

    QCOMPARE(editor.toPlainText(), QString("Hello World"));
}

void TestProtectedRanges::TestProtectionPolicyWarn() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.SetProtection(0, 5, "test", "target");

    bool handlerCalled = false;
    editor.SetProtectionViolationHandler(
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

void TestProtectedRanges::TestHandler() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtectionPolicy(Rte::ProtectionPolicy::Warn);
    editor.SetProtection(0, 5, "test", "target");

    std::string receivedType;
    std::string receivedTarget;
    bool handlerCalled = false;
    editor.SetProtectionViolationHandler(
        [&](const Rte::ProtectedRangeInfo& info,
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

void TestProtectedRanges::TestAllProtection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC DEF GHI");

    editor.SetProtection(0, 3, "type1", "target1");
    editor.SetProtection(5, 8, "type2", "target2");

    std::vector<Rte::ProtectedRangeInfo> all = editor.AllProtection();
    QCOMPARE(all.size(), static_cast<std::size_t>(2));
    QCOMPARE(all[0].type, std::string("type1"));
    QCOMPARE(all[0].target, std::string("target1"));
    QCOMPARE(all[1].type, std::string("type2"));
    QCOMPARE(all[1].target, std::string("target2"));
}

void TestProtectedRanges::TestIsProtected() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Test");

    QVERIFY(!editor.IsProtected(0));
    QVERIFY(!editor.IsProtected(1));

    editor.SetProtection(1, 3, "test", "target");
    QVERIFY(!editor.IsProtected(0));
    QVERIFY(editor.IsProtected(1));
    QVERIFY(editor.IsProtected(2));
    QVERIFY(!editor.IsProtected(3));
}

void TestProtectedRanges::TestClearProtection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC");

    editor.SetProtection(0, 3, "test", "target");
    QCOMPARE(editor.AllProtection().size(), static_cast<std::size_t>(1));

    editor.ClearProtection();
    QCOMPARE(editor.AllProtection().size(), static_cast<std::size_t>(0));
    QVERIFY(!editor.IsProtected(0));
}

void TestProtectedRanges::TestKeyPressBackspace() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    editor.keyPressEvent(&event);
}

QTEST_MAIN(TestProtectedRanges)
#include "TestProtectedRanges.moc"
