# QtRtfEditor

Wiederverwendbare **RTF-faehige QTextEdit-Subclass** fuer Qt6.

## Features

- **RTF-Ein- und -Ausgabe**: Laden und Serialisieren von RTF-Daten
  (Delphi/TRichEdit-kompatibel).
- **Geschuetzte Textbereiche**: Konfigurierbarer Schutz von
  Textblöcken vor Loeschoperationen (None, Warn, Block).
- **Subclassing**: Alle kritischen Methoden sind virtuell,
  um anwendungsspezifische Erweiterungen zu ermoglichen.
- **Dual-Licensing**: LGPL-3.0-or-later oder kommerzielle Lizenz.

## Schnellstart

```bash
# Bauen
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Demo starten
./build/examples/demo/demo

# Tests
cd build
ctest --output-on-failure
```

## Integration in CMake-Projekte

```cmake
# CMakeLists.txt Ihres Projekts
include(FetchContent)
FetchContent_Declare(QtRtfEditor
    GIT_REPOSITORY https://github.com/Fritz1729/QtRtfEditor.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(QtRtfEditor)

# Target verlinken
target_link_libraries(MeinTarget
    PRIVATE QtRtfEditor::QtRtfEditor
)
```

Alternativ nach `make install`:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(QtRtfEditor REQUIRED)
target_link_libraries(MeinTarget PRIVATE QtRtfEditor::QtRtfEditor)
```

## Beispiel-Code

```cpp
#include <RichTextEdit/RichTextEdit.h>

Rte::RichTextEdit editor;
editor.setProtectionPolicy(Rte::ProtectionPolicy::Warn);

// RTF laden
std::string rtf = R"({\rtf1\ansi{\b Fett}{\b0 normal})";
editor.laden(rtf, Rte::FormatMode::Rtf);

// Schutz setzen
editor.setzeSchutz(0, 4, "Lexikon", "Schlagwort:Beispiel");

// RTF speichern
std::string gespeichert = editor.speichern(Rte::FormatMode::Rtf);
```

## Subclassing

```cpp
class MvEditor : public Rte::RichTextEdit
{
protected:
    void pruefeSchutz(const QTextCursor &cursor, bool &erlaubt) override
    {
        // MV-spezifische Logik: Schutztypen übersetzen,
        // eigene Dialoge anzeigen
        Rte::RichTextEdit::pruefeSchutz(cursor, erlaubt);
    }
};
```

## Lizenz

Dual-Licensing: **LGPL-3.0-or-later** oder **kommerzielle Lizenz**.
Siehe [Qt Licensing](https://www.qt.io/licensing) fuer Details.
