#include "RtfInputReader.h"

namespace Rte {

bool InputReader::Matches(std::string_view s) const {
    if (_pos + s.size() > _input.size()) return false;
    if (_input.compare(_pos, s.size(), s) != 0) return false;
    if (_pos + s.size() < _input.size() && IsWordChar(_input[_pos + s.size()])) return false;
    return true;
}

bool InputReader::ConsumeMatch(std::string_view s, bool skipWhitespace) {
    if (Matches(s)) {
        _pos += s.size();
        if (skipWhitespace) SkipWhitespace();
        return true;
    }
    return false;
}

int InputReader::ParseInt() {
    int val = 0;
    while (!IsEof() && IsDigit(_input[_pos])) {
        val = val * 10 + (_input[_pos] - '0');
        _pos++;
    }
    return val;
}

std::pair<std::string, int> InputReader::ReadControlWord() {
    std::string word;
    int arg = 0;
    bool hasArg = false;
    while (!IsEof() && (IsWordChar(_input[_pos]) || IsDigit(_input[_pos]))) {
        if (_input[_pos] >= '0' && _input[_pos] <= '9') {
            arg = arg * 10 + (_input[_pos] - '0');
            hasArg = true;
        } else {
            word += static_cast<char>(static_cast<unsigned char>(_input[_pos]) | 0x20);
        }
        _pos++;
    }
    return {word, hasArg ? arg : -1};
}

void InputReader::ConsumeControlDelimiter(int arg, bool hasArg) {
    (void)arg;
    if (hasArg && !IsEof() && !IsWordChar(_input[_pos]) && _input[_pos] != '\\' &&
            _input[_pos] != '}' && _input[_pos] != '{') {
        _pos++;
    } else if (!hasArg && !IsEof() && _input[_pos] == ' ') {
        _pos++;
    }
}

void InputReader::SkipAs(char expected) {
    if (IsEof() || _input[_pos] != expected) {
        throw std::runtime_error(std::string("unexpected input, expected '") + expected + "'");
    }
    _pos++;
}

void InputReader::SkipWhitespace() {
    while (!IsEof() && IsWhitespace(_input[_pos])) {
        _pos++;
    }
}

void InputReader::SkipDigits() {
    while (!IsEof() && IsDigit(_input[_pos])) {
        _pos++;
    }
}

std::string InputReader::ReadStarDestWord() {
    if (!PeekIs("\\*")) return {};
    size_t saved = Pos();
    AdvanceBy(2);
    SkipWhitespace();
    std::string word;
    if (!IsEof() && PeekIs('\\')) {
        Advance();
        SkipWhitespace();
        if (!IsEof() && Rte::IsWordChar(Peek())) {
            auto [w, a] = ReadControlWord();
            word = w;
        }
    }
    Seek(saved);
    return word;
}

} // namespace Rte
