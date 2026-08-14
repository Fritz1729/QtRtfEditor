#pragma once

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Rte {

[[nodiscard]] constexpr bool IsWordChar(char c) {
    return std::isalpha(static_cast<unsigned char>(c));
}

[[nodiscard]] constexpr bool IsDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

[[nodiscard]] constexpr bool IsWhitespace(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

[[nodiscard]] constexpr bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

class InputReader {
    std::string_view _input;
    size_t _pos = 0;

public:
    InputReader() = default;
    explicit InputReader(std::string_view input) : _input(input) {}

    [[nodiscard]] bool IsEof() const { return _pos >= _input.size(); }
    [[nodiscard]] size_t Pos() const { return _pos; }
    [[nodiscard]] char Peek() const { return _input[_pos]; }
    [[nodiscard]] constexpr bool PeekIs(char c, size_t offset = 0) const {
        return _pos + offset < _input.size() && _input[_pos + offset] == c;
    }

    [[nodiscard]] constexpr bool PeekIs(std::string_view s) const {
        if (_pos + s.size() > _input.size()) return false;
        return _input.compare(_pos, s.size(), s) == 0;
    }

    template<typename Pred>
    [[nodiscard]] constexpr bool PeekIf(Pred pred, size_t offset = 0) const {
        return _pos + offset < _input.size() ? pred(_input[_pos + offset]) : false;
    }

    char Advance() { return _input[_pos++]; }
    void AdvanceBy(size_t n) { _pos += n; }
    bool AdvanceIf(char c) {
        if (_pos < _input.size() && _input[_pos] == c) { _pos++; return true; }
        return false;
    }
    template<typename Pred>
    bool AdvanceIf(Pred pred) {
        if (_pos < _input.size() && pred(_input[_pos])) { _pos++; return true; }
        return false;
    }

    void SkipAs(char expected);
    void Seek(size_t pos) { _pos = pos; }

    bool ConsumeMatch(std::string_view s, bool skipWhitespace = false);

    int ParseInt();
    std::pair<std::string, int> ReadControlWord();

    void ConsumeControlDelimiter(int arg, bool hasArg);
    void SkipWhitespace();
    void SkipDigits();

    [[nodiscard]] std::string Substring(size_t start, size_t end) const {
        return std::string(_input.substr(start, end - start));
    }

private:
    [[nodiscard]] bool Matches(std::string_view s) const;
};

} // namespace Rte
