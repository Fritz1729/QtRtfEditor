#include "ProtectedRange.h"

namespace Rte {

ProtectedRange::ProtectedRange(std::size_t start, std::size_t end,
                               std::string type, std::string target)
    : _start(start), _end(end), _type(std::move(type)),
      _target(std::move(target))
{
}

std::size_t ProtectedRange::start() const { return _start; }
std::size_t ProtectedRange::end() const { return _end; }

const std::string &ProtectedRange::type() const { return _type; }
const std::string &ProtectedRange::target() const { return _target; }

bool ProtectedRange::encompasses(std::size_t position) const {
    return position >= _start && position < _end;
}

bool ProtectedRange::overlaps(std::size_t start, std::size_t end) const {
    return start < _end && end > _start;
}

} // namespace Rte
