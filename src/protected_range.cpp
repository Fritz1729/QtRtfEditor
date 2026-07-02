// src/protected_range.cpp

#include "protected_range.h"

namespace Rte {

ProtectedRange::ProtectedRange(std::size_t start, std::size_t ende,
                               std::string typ, std::string ziel)
    : _start(start), _ende(ende), _typ(std::move(typ)),
      _ziel(std::move(ziel))
{
}

std::size_t ProtectedRange::start() const { return _start; }
std::size_t ProtectedRange::ende() const { return _ende; }

const std::string &ProtectedRange::typ() const { return _typ; }
const std::string &ProtectedRange::ziel() const { return _ziel; }

bool ProtectedRange::umfasst(std::size_t position) const {
    return position >= _start && position < _ende;
}

bool ProtectedRange::ueberschneidet(std::size_t start, std::size_t ende) const {
    return start < _ende && ende > _start;
}

} // namespace Rte
