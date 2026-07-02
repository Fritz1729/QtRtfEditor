// examples/demo/main.cpp
//
// Minimaler Demonstrator fuer QtRtfEditor.
//
// Kompilierung:
//   cmake -B build -DCMAKE_BUILD_TYPE=Debug
//   cmake --build build
//   ./build/examples/demo/demo

#include <RichTextEdit/RichTextEdit.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QStatusBar>

class DemoWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DemoWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("QtRtfEditor Demo");
        setCentralWidget(&_editor);
        statusBar()->show();
        statusBar()->showMessage("Bereit");

        menue();

        // Beispiel: Delphi-kompatiblen RTF laden
        std::string beispielRtf = R"({\rtf1\ansi\deff0
{\colortbl ;\red255\green0\blue0;\red0\green128\blue0;}
{\fonttbl{\f0\froman\fcharset0 Times New Roman;}{\f1\fswiss\fcharset0 Arial;}}
\f0\fs24\ql
\b Medienverwaltung\b0 Beispiel\r\n
\r\n
\f1\fs20 Dieser \i Editor\i0 unterstuetzt \ul RTF\ul0 und \cf1 geschuetzte\cf0 Verweise.\r\n
})";
        try {
            _editor.laden(beispielRtf, Rte::FormatMode::Rtf);
            statusBar()->showMessage("Beispiel-RTF geladen");
        } catch (const std::exception &e) {
            statusBar()->showMessage(QString("Fehler: %1").arg(e.what()));
        }
    }

private:
    void menue() {
        QMenuBar *menueBar = menuBar();

        // Datei-Menue
        QMenu *datei = menueBar->addMenu("&Datei");

        QAction *laden = datei->addAction("&RTF laden...");
        connect(laden, &QAction::triggered, this, &DemoWindow::dateiLaden);

        QAction *speichern = datei->addAction("RTF &speichern...");
        connect(speichern, &QAction::triggered, this, &DemoWindow::dateiSpeichern);

        datei->addSeparator();

        QAction *beenden = datei->addAction("Be&enden");
        connect(beenden, &QAction::triggered, this, &QApplication::quit);

        // Format-Menue
        QMenu *format = menueBar->addMenu("&Format");

        QAction *fett = format->addAction("&Fett");
        fett->setShortcut(Qt::CTRL | Qt::Key_B);
        connect(fett, &QAction::triggered, this, [this] {
            QTextCharFormat fmt;
            fmt.setFontWeight(fmt.fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
            mergeFormatOnSelection(fmt);
        });

        QAction *kursiv = format->addAction("K&ursiv");
        kursiv->setShortcut(Qt::CTRL | Qt::Key_I);
        connect(kursiv, &QAction::triggered, this, [this] {
            QTextCharFormat fmt;
            fmt.setFontItalic(!fmt.fontItalic());
            mergeFormatOnSelection(fmt);
        });

        QAction *unterstrichen = format->addAction("&Unterstrichen");
        unterstrichen->setShortcut(Qt::CTRL | Qt::Key_U);
        connect(unterstrichen, &QAction::triggered, this, [this] {
            QTextCharFormat fmt;
            fmt.setFontUnderline(!fmt.fontUnderline());
            mergeFormatOnSelection(fmt);
        });

        // Schutz-Menue
        QMenu *schutz = menueBar->addMenu("&Schutz");

        QAction *schutzSetzen = schutz->addAction("Schutz &setzen...");
        connect(schutzSetzen, &QAction::triggered, this, &DemoWindow::schutzSetzen);

        QAction *schutzLoeschen = schutz->addAction("Schutz &loeschen");
        connect(schutzLoeschen, &QAction::triggered, this, [this] {
            _editor.loescheSchutz();
            statusBar()->showMessage("Alle Schutz-Bereiche geloescht");
        });

        // Hilfe-Menue
        QMenu *hilfe = menueBar->addMenu("&Hilfe");

        QAction *ueber = hilfe->addAction("&About QtRtfEditor");
        connect(ueber, &QAction::triggered, this, [this] {
            QMessageBox::about(this, "About QtRtfEditor",
                "QtRtfEditor Demo\n"
                "Wiederverwendbare RTF-faehige QTextEdit-Subclass\n"
                "\n"
                "Lizenz: Dual (LGPL-3.0+ / kommerziell)");
        });
    }

    void dateiLaden() {
        QString pfad = QFileDialog::getOpenFileName(this,
            "RTF laden", "", "RTF-Dateien (*.rtf);;Alle Dateien (*)");
        if (pfad.isEmpty()) return;

        QFile file(pfad);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Fehler",
                QString("Datei kann nicht geoeffnet werden:\n%1").arg(pfad));
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        try {
            _editor.laden(data.toStdString(), Rte::FormatMode::Rtf);
            statusBar()->showMessage(QString("Geladen: %1").arg(pfad));
        } catch (const std::exception &e) {
            QMessageBox::critical(this, "Fehler", e.what());
        }
    }

    void dateiSpeichern() {
        QString pfad = QFileDialog::getSaveFileName(this,
            "RTF speichern", "", "RTF-Dateien (*.rtf);;Alle Dateien (*)");
        if (pfad.isEmpty()) return;

        if (!pfad.endsWith(".rtf", Qt::CaseInsensitive)) {
            pfad += ".rtf";
        }

        std::string rtf = _editor.speichern(Rte::FormatMode::Rtf);

        QFile file(pfad);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Fehler",
                QString("Datei kann nicht geschrieben werden:\n%1").arg(pfad));
            return;
        }

        file.write(rtf.c_str(), static_cast<QInt64>(rtf.size()));
        file.close();

        statusBar()->showMessage(QString("Gespeichert: %1").arg(pfad));
    }

    void schutzSetzen() {
        QTextCursor cursor = _editor.textCursor();
        if (!cursor.hasSelection()) {
            statusBar()->showMessage("Bitte markieren Sie zuerst den zu schuetzenden Text");
            return;
        }

        bool ok;
        QString typ = QInputDialog::getText(this, "Schutz setzen",
            "Typ (z. B. 'Lexikon', 'Person'):", QLineEdit::Normal,
            "Lexikon", &ok);
        if (!ok || typ.isEmpty()) return;

        QString ziel = QInputDialog::getText(this, "Schutz setzen",
            "Ziel-Referenz (z. B. 'Schlagwort:Beispiel'):", QLineEdit::Normal,
            "", &ok);
        if (!ok) return;

        std::size_t start = static_cast<std::size_t>(cursor.selectionStart());
        std::size_t ende = static_cast<std::size_t>(cursor.selectionEnd());

        _editor.setzeSchutz(start, ende, typ.toStdString(), ziel.toStdString());
        statusBar()->showMessage(QString("Schutz gesetzt: [%1] %2").arg(typ).arg(ziel));
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
