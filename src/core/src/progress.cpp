#include "bb/core/progress.hpp"

#include <sstream>

namespace bb::core {

std::string Progress::serialize() const {
    std::ostringstream out;
    out << "blipblop-progress 1\n";
    out << "missions " << missionsReached << "\n";
    return out.str();
}

std::optional<Progress> Progress::parse(std::string_view text, std::string& error) {
    Progress p;
    bool header = false;
    std::size_t lineNumber = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) {
            eol = text.size();
        }
        std::istringstream line(std::string(text.substr(pos, eol - pos)));
        pos = eol + 1;
        ++lineNumber;
        std::string name;
        std::string value;
        if (!(line >> name) || name[0] == '#') {
            continue;
        }
        line >> value;
        const std::string where = "line " + std::to_string(lineNumber) + ": ";
        if (!header) {
            if (name != "blipblop-progress" || value != "1") {
                error = where + "expected 'blipblop-progress 1'";
                return std::nullopt;
            }
            header = true;
            continue;
        }
        if (name == "missions") {
            int count = 0;
            const bool digits = !value.empty() && value.size() <= 2 &&
                                value.find_first_not_of("0123456789") == std::string::npos;
            if (!digits || (count = std::stoi(value)) < 1 || count > kAllMissions) {
                error = where + "missions must be 1 to " + std::to_string(kAllMissions);
                return std::nullopt;
            }
            p.missionsReached = count;
        }
    }
    if (!header) {
        error = "empty file: expected 'blipblop-progress 1'";
        return std::nullopt;
    }
    return p;
}

}  // namespace bb::core
