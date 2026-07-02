#include "RtfImport.h"

#include <QString>

namespace Rte {

namespace {

static const char* delphiTags[] = {
    "\\b", "\\i", "\\ul", "\\cf", "\\fs", "\\f",
    "\\ql", "\\qc", "\\qr", "\\li", "\\fi", "\\ri",
    "\\super", "\\sub", nullptr
};

} // namespace

bool isDelphiCompatible(const std::string &rtf) {
    if (rtf.empty()) {
        return false;
    }

    QString rtfStr = QString::fromUtf8(rtf.c_str(), static_cast<int>(rtf.size()));

    for (int i = 0; delphiTags[i]; ++i) {
        if (rtfStr.contains(delphiTags[i])) {
            return true;
        }
    }
    return false;
}

} // namespace Rte
