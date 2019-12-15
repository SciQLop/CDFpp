#include <string>
#include <optional>
#include <unordered_map>
#include "attribute.hpp"
#include "variable.hpp"
#include "cdf-io.hpp"

namespace cdf {
    struct CDF
    {
        std::unordered_map<std::string, Variable> variables;
        std::unordered_map<std::string, Attribute> attrinutes;
        const Variable& operator[](const std::string& name)const
        {
            return variables.at(name);
        }
    };

    std::optional<CDF> open(const std::string& path)
    {
        return std::nullopt;
    }

}
