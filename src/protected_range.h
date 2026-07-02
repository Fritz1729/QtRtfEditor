#pragma once
// src/protected_range.h
// Geschützte Textbereiche in RichTextEdit.
//
// Jeder Schutz repraesentiert einen zusammenhaengenden
// Textbereich [start, ende) mit beliebigen Metadaten.
// Schuetzbare Operationen werden durch die Schutzrichtlinie
// gesteuert (None, Warn, Block).

#include <cstddef>
#include <string>
#include <vector>

namespace Rte {

struct SchutzInfo {
    std::size_t start;        // Start-Position im Dokument (0-indexiert)
    std::size_t ende;         // Ende-Position (exkl.)
    std::string typ;          // Typ-Bezeichnung (z. B. "Lexikon", "Person")
    std::string ziel;         // Ziel-Referenz (z. B. "Schlagwort:Beispiel")
};

class ProtectedRange {
public:
    ProtectedRange(std::size_t start, std::size_t ende,
                   std::string typ, std::string ziel);

    [[nodiscard]] std::size_t start() const;
    [[nodiscard]] std::size_t ende() const;
    [[nodiscard]] const std::string &typ() const;
    [[nodiscard]] const std::string &ziel() const;

    [[nodiscard]] bool umfasst(std::size_t position) const;
    [[nodiscard]] bool ueberschneidet(std::size_t start, std::size_t ende) const;

private:
    std::size_t _start;
    std::size_t _ende;
    std::string _typ;
    std::string _ziel;
};

} // namespace Rte
