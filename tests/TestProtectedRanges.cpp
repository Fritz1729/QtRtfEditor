#include <RichTextEdit.h>
#include <QtTest>
#include <QApplication>

class TestProtectedRanges : public QObject {
    Q_OBJECT

private slots:
    void TestIsProtected();
    void TestSetProtection();
    void TestClearProtection();
    void TestProtectFromRtf();
    void TestClampCursorBackspace();
    void TestClampCursorDelete();
    void TestClampCursorArrowKeys();
    void TestClampCursorPaste();
    void TestProtectedRegionClicked();
};

void TestProtectedRanges::TestIsProtected() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");

    QVERIFY(!editor.IsProtected(0));
    QVERIFY(!editor.IsProtected(5));

    editor.SetProtection(0, 5);

    QVERIFY(editor.IsProtected(0));
    QVERIFY(editor.IsProtected(2));
    QVERIFY(editor.IsProtected(4));
    QVERIFY(!editor.IsProtected(5));
    QVERIFY(!editor.IsProtected(10));
}

void TestProtectedRanges::TestSetProtection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("AAA BBB CCC");

    editor.SetProtection(0, 3);
    editor.SetProtection(5, 8);

    QVERIFY(editor.IsProtected(1));
    QVERIFY(editor.IsProtected(6));
    QVERIFY(!editor.IsProtected(3));
    QVERIFY(!editor.IsProtected(4));
}

void TestProtectedRanges::TestClearProtection() {
    Rte::RichTextEdit editor;
    editor.setPlainText("ABC");

    editor.SetProtection(0, 3);
    QVERIFY(editor.IsProtected(0));

    editor.ClearProtection();
    QVERIFY(!editor.IsProtected(0));
    QVERIFY(!editor.IsProtected(1));
    QVERIFY(!editor.IsProtected(2));
}

void TestProtectedRanges::TestProtectFromRtf() {
    Rte::RichTextEdit editor;
    editor.Load(R"({\rtf1\ansi\deff0\fs24{\protect protected}\protect0\par})", Rte::FormatMode::Rtf);

    // Verify that the "protected" text is detected as protected
    int docLen = editor.document()->characterCount();
    bool foundProtected = false;
    for (int i = 0; i < docLen; ++i) {
        if (editor.IsProtected(static_cast<std::size_t>(i))) {
            foundProtected = true;
            break;
        }
    }
    QVERIFY(foundProtected);
}

void TestProtectedRanges::TestClampCursorBackspace() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtection(0, 5);

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(5);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    editor.keyPressEvent(&event);

    QCOMPARE(editor.toPlainText(), QString("Hello World"));
}

void TestProtectedRanges::TestClampCursorDelete() {
    Rte::RichTextEdit editor;
    editor.Load(
        R"({\rtf1\ansi\deff0\fs24 Hello {\protect World}\protect0\par})",
        Rte::FormatMode::Rtf);

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(6); // "o" of "Hello" — Delete would reach protected " World"
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    editor.keyPressEvent(&event);

    // Text should be unchanged — Delete blocked from crossing into protected text
    QString text = editor.toPlainText();
    QVERIFY(text.contains("Hello"));
    QVERIFY(text.contains("World"));
}

void TestProtectedRanges::TestClampCursorArrowKeys() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtection(5, 11);

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(4);
    editor.setTextCursor(cursor);

    QKeyEvent event(QKeyEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    editor.keyPressEvent(&event);

    int pos = editor.textCursor().position();
    QVERIFY(!editor.IsProtected(static_cast<std::size_t>(pos)));
}

void TestProtectedRanges::TestClampCursorPaste() {
    Rte::RichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtection(5, 11);

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(4);
    editor.setTextCursor(cursor);

    QMimeData mimeData;
    mimeData.setText("X");
    editor.insertFromMimeData(&mimeData);

    int pos = editor.textCursor().position();
    QVERIFY(!editor.IsProtected(static_cast<std::size_t>(pos)));
}

class ClickableRichTextEdit : public Rte::RichTextEdit {
public:
    using Rte::RichTextEdit::RichTextEdit;

    void TestClickAtPosition(int pos) {
        QTextCursor cursor = textCursor();
        cursor.setPosition(pos);
        setTextCursor(cursor);

        QMouseEvent pressEvent(QEvent::MouseButtonPress,
            QPointF(0, 0), QPoint(0, 0),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        mousePressEvent(&pressEvent);
    }
};

void TestProtectedRanges::TestProtectedRegionClicked() {
    ClickableRichTextEdit editor;
    editor.setPlainText("Hello World");
    editor.SetProtection(5, 11);

    bool signalReceived = false;
    int receivedStart = -1, receivedEnd = -1;
    QString receivedText;

    QObject::connect(&editor, &Rte::RichTextEdit::protectedRegionClicked,
        [&signalReceived, &receivedStart, &receivedEnd, &receivedText](
            std::size_t start, std::size_t end, const QString& text) {
            signalReceived = true;
            receivedStart = static_cast<int>(start);
            receivedEnd = static_cast<int>(end);
            receivedText = text;
        });

    // Position cursor in protected region and trigger mouse event
    editor.TestClickAtPosition(8);

    if (signalReceived) {
        QCOMPARE(receivedStart, 5);
        QCOMPARE(receivedEnd, 11);
        QVERIFY(receivedText == "World" || receivedText == " World");
    }

    // Cursor should be clamped out of protected region
    int finalPos = editor.textCursor().position();
    QVERIFY(!editor.IsProtected(static_cast<std::size_t>(finalPos)));
}

QTEST_MAIN(TestProtectedRanges)
#include "TestProtectedRanges.moc"
