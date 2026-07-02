#pragma once

// Protected text ranges in RichTextEdit.
//
// Each range represents a contiguous text span [start, end)
// with arbitrary metadata. Protected operations are governed
// by a protection policy (None, Warn, Block).

#include <cstddef>
#include <string>

namespace Rte {

/**
 * @brief Information about a protected text range.
 */
struct ProtectedRangeInfo {
    std::size_t start;        // Start position in document (0-indexed)
    std::size_t end;          // End position (exclusive)
    std::string type;         // Type label (e.g., "lexicon", "person")
    std::string target;       // Target reference (e.g., "entry:Example")
};

// Internal representation of a protected range.
class ProtectedRange {
public:
    ProtectedRange(std::size_t start, std::size_t end,
                   std::string type, std::string target);

    [[nodiscard]] std::size_t start() const;
    [[nodiscard]] std::size_t end() const;
    [[nodiscard]] const std::string &type() const;
    [[nodiscard]] const std::string &target() const;

    [[nodiscard]] bool Encompasses(std::size_t position) const;
    [[nodiscard]] bool Overlaps(std::size_t start, std::size_t end) const;

private:
    std::size_t _start;
    std::size_t _end;
    std::string _type;
    std::string _target;
};

} // namespace Rte
