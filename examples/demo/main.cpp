#include <QStatusBar>
#include <RichTextEdit.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFont>

class DemoWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DemoWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("QtRtfEditor Demo");
        setCentralWidget(&_editor);
        statusBar()->showMessage("Ready");

        setupMenu();

        std::string sampleRtf = R"({\rtf1\ansi\deff0
{\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
{\fonttbl{\f0\froman\fcharset0 Times New Roman;}
         {\f1\fswiss\fcharset0 Arial;}}
\f0\fs24\ql
\b Medienverwaltung\b0 example\r\n
\r\n
\f1\fs20 This \i editor\i0 supports \ul RTF\ul0 and \cf1 protected\cf0 references.\r\n
})";
        try {
            _editor.load(sampleRtf, Rte::FormatMode::Rtf);
            statusBar()->showMessage("Sample RTF loaded");
        } catch (const std::exception &e) {
            statusBar()->showMessage(QString("Error: %1").arg(e.what()));
        }
    }

private:
    void setupMenu() {
        QMenuBar *bar = menuBar();

        QMenu *file = bar->addMenu("&File");

        QAction *load = file->addAction("&Load RTF...");
        connect(load, &QAction::triggered, this, &DemoWindow::loadFile);

        QAction *save = file->addAction("Save &RTF...");
        connect(save, &QAction::triggered, this, &DemoWindow::saveFile);

        file->addSeparator();

        QAction *quit = file->addAction("E&xit");
        connect(quit, &QAction::triggered, this, &QApplication::quit);

        QMenu *format = bar->addMenu("&Format");

        QAction *bold = format->addAction("&Bold");
        bold->setShortcut(Qt::CTRL | Qt::Key_B);
        connect(bold, &QAction::triggered, this, [this] {
            QTextCharFormat fmt;
            fmt.setFontWeight(fmt.fontWeight() == QFont::Bold
                              ? QFont::Normal : QFont::Bold);
            mergeFormatOnSelection(fmt);
        });

        QMenu *protection = bar->addMenu("&Protection");

        QAction *setProt = protection->addAction("Set &protection...");
        connect(setProt, &QAction::triggered, this, &DemoWindow::setProtection);

        QAction *clearProt = protection->addAction("Clear &protection");
        connect(clearProt, &QAction::triggered, this, [this] {
            _editor.clearProtection();
            statusBar()->showMessage("All protection ranges cleared");
        });

        QMenu *help = bar->addMenu("&Help");

        QAction *about = help->addAction("&About QtRtfEditor");
        connect(about, &QAction::triggered, this, [this] {
            QMessageBox::about(this, "About QtRtfEditor",
                "QtRtfEditor Demo\n"
                "Reusable RTF-capable QTextEdit subclass\n"
                "\n"
                "License: Dual (LGPL-3.0+ / commercial)");
        });
    }

    void loadFile() {
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
            _editor.load(data.toStdString(), Rte::FormatMode::Rtf);
            statusBar()->showMessage(QString("Loaded: %1").arg(path));
        } catch (const std::exception &e) {
            QMessageBox::critical(this, "Error", e.what());
        }
    }

    void saveFile() {
        QString path = QFileDialog::getSaveFileName(
            this, "Save RTF", "", "RTF Files (*.rtf);;All Files (*)");
        if (path.isEmpty()) return;

        if (!path.endsWith(".rtf", Qt::CaseInsensitive)) {
            path += ".rtf";
        }

        std::string rtf = _editor.save(Rte::FormatMode::Rtf);

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Error",
                QString("Cannot write file:\n%1").arg(path));
            return;
        }

        file.write(rtf.c_str(), static_cast<qint64>(rtf.size()));
        file.close();

        statusBar()->showMessage(QString("Saved: %1").arg(path));
    }

    void setProtection() {
        QTextCursor cursor = _editor.textCursor();
        if (!cursor.hasSelection()) {
            statusBar()->showMessage("Please select text to protect first");
            return;
        }

        bool ok;
        QString type = QInputDialog::getText(
            this, "Set Protection",
            "Type (e.g., 'lexicon', 'person'):",
            QLineEdit::Normal, "lexicon", &ok);
        if (!ok || type.isEmpty()) return;

        QString target = QInputDialog::getText(
            this, "Set Protection",
            "Target reference (e.g., 'entry:Example'):",
            QLineEdit::Normal, "", &ok);
        if (!ok) return;

        std::size_t start = static_cast<std::size_t>(
            cursor.selectionStart());
        std::size_t end = static_cast<std::size_t>(
            cursor.selectionEnd());

        _editor.setProtection(start, end, type.toStdString(),
                              target.toStdString());
        statusBar()->showMessage(
            QString("Protection set: [%1] %2").arg(type).arg(target));
    }

    void mergeFormatOnSelection(const QTextCharFormat &format) {
        QTextCursor cursor = _editor.textCursor();
        if (cursor.hasSelection()) {
            cursor.mergeCharFormat(format);
            _editor.setTextCursor(cursor);
        }
    }

    Rte::RichTextEdit _editor;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    DemoWindow window;
    window.resize(800, 600);
    window.show();

    return app.exec();
}

#include "main.moc"
