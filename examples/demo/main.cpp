#include "DemoWindow.h"
#include "ToolbarBuilder.h"
#include "MenuBuilder.h"
#include "RecentFileHandler.h"
#include <QStatusBar>
#include <RichTextEdit.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFont>
#include <QSettings>

QMenu* DemoWindow::RecentFilesMenu() const {
    return _pMenuBuilder->RecentFilesMenu();
}

DemoWindow::DemoWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("QtRtfEditor Demo");
    setCentralWidget(&_editor);
    statusBar()->showMessage("Ready");

    QToolBar* toolbar = addToolBar("Formatting");
    _pToolbarBuilder = new ToolbarBuilder(toolbar, this);
    _pToolbarBuilder->Build();

    _pMenuBuilder = new MenuBuilder(menuBar(), this);
    _pMenuBuilder->Build();

    _pRecentFileHandler = new RecentFileHandler(this, this);
}

void DemoWindow::MergeFormatOnSelection(const QTextCharFormat& format) {
    QTextCursor cursor = _editor.textCursor();
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(format);
    } else {
        cursor.setCharFormat(format);
    }
    _editor.setTextCursor(cursor);
}

void DemoWindow::ApplyAlignment(Qt::Alignment alignment) {
    QTextCursor cursor = _editor.textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();

    switch (alignment) {
        case Qt::AlignLeft:
            fmt.setAlignment(Qt::AlignLeft);
            break;
        case Qt::AlignHCenter:
            fmt.setAlignment(Qt::AlignHCenter);
            break;
        case Qt::AlignRight:
            fmt.setAlignment(Qt::AlignRight);
            break;
        case Qt::AlignJustify:
            fmt.setAlignment(Qt::AlignJustify);
            break;
        default:
            break;
    }

    cursor.setBlockFormat(fmt);
    _editor.setTextCursor(cursor);
}

void DemoWindow::ApplyIndent(int delta) {
    QTextCursor cursor = _editor.textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    qreal indent = fmt.textIndent() + delta;
    if (indent < 0) indent = 0;
    fmt.setTextIndent(indent);
    cursor.setBlockFormat(fmt);
    _editor.setTextCursor(cursor);
}

void DemoWindow::InsertSpecialChar(QChar c) {
    QTextCursor cursor = _editor.textCursor();
    cursor.insertText(c);
    _editor.setTextCursor(cursor);
}

void DemoWindow::ToggleWordWrap(bool checked) {
    if (checked) {
        _editor.setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    } else {
        _editor.setWordWrapMode(QTextOption::NoWrap);
    }
}

void DemoWindow::ToggleBold(bool checked) {
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    MergeFormatOnSelection(fmt);
}

void DemoWindow::ToggleItalic(bool checked) {
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    MergeFormatOnSelection(fmt);
}

void DemoWindow::ToggleUnderline(bool checked) {
    QTextCharFormat fmt;
    fmt.setFontUnderline(checked);
    MergeFormatOnSelection(fmt);
}

void DemoWindow::ToggleStrikethrough(bool checked) {
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(checked);
    MergeFormatOnSelection(fmt);
}

void DemoWindow::ToggleSuperscript(bool checked) {
    applyFormatOnSelection([checked](QTextCharFormat& fmt) {
        if (checked) {
            fmt.setVerticalAlignment(QTextCharFormat::VerticalAlignment::AlignSuperScript);
        } else if (fmt.verticalAlignment() == QTextCharFormat::VerticalAlignment::AlignSuperScript) {
            fmt.setVerticalAlignment(QTextCharFormat::VerticalAlignment::AlignNormal);
        }
    });
}

void DemoWindow::ToggleSubscript(bool checked) {
    applyFormatOnSelection([checked](QTextCharFormat& fmt) {
        if (checked) {
            fmt.setVerticalAlignment(QTextCharFormat::VerticalAlignment::AlignSubScript);
        } else if (fmt.verticalAlignment() == QTextCharFormat::VerticalAlignment::AlignSubScript) {
            fmt.setVerticalAlignment(QTextCharFormat::VerticalAlignment::AlignNormal);
        }
    });
}

void DemoWindow::LoadFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load RTF", "", "RTF Files (*.rtf);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            QString("Cannot open file:\n%1").arg(path));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    try {
        _editor.Load(data.toStdString(), Rte::FormatMode::Rtf);
        statusBar()->showMessage(QString("Loaded: %1").arg(path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", e.what());
    }

    _pRecentFileHandler->AddRecentFile(path);
}

void DemoWindow::SaveFile() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save RTF", "", "RTF Files (*.rtf);;All Files (*)");
    if (path.isEmpty()) return;

    if (!path.endsWith(".rtf", Qt::CaseInsensitive)) {
        path += ".rtf";
    }

    std::string rtf = _editor.Save(Rte::FormatMode::Rtf);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            QString("Cannot write file:\n%1").arg(path));
        return;
    }

    file.write(rtf.c_str(), static_cast<qint64>(rtf.size()));
    file.close();

    statusBar()->showMessage(QString("Saved: %1").arg(path));

    _pRecentFileHandler->AddRecentFile(path);
}

void DemoWindow::ClearRecentFiles() {
    _pRecentFileHandler->ClearRecentFiles();
}

void DemoWindow::SetProtection() {
    QTextCursor cursor = _editor.textCursor();
    if (!cursor.hasSelection()) {
        statusBar()->showMessage("Please select text to protect first");
        return;
    }

    std::size_t start = static_cast<std::size_t>(
        cursor.selectionStart());
    std::size_t end = static_cast<std::size_t>(
        cursor.selectionEnd());

    _editor.SetProtection(start, end);
    statusBar()->showMessage("Protection set on selected text");
}

template<typename Fn>
void DemoWindow::applyFormatOnSelection(Fn fn) {
    QTextCursor cursor = _editor.textCursor();
    if (cursor.hasSelection()) {
        QTextCharFormat fmt = cursor.charFormat();
        fn(fmt);
        cursor.mergeCharFormat(fmt);
        _editor.setTextCursor(cursor);
    }
}

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName("QtRtfEditor");
    QApplication::setApplicationName("Demo");
    QApplication app(argc, argv);

    DemoWindow window;
    window.resize(900, 650);
    window.show();

    return app.exec();
}
