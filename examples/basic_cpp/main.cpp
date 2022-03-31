#include "cdfpp/cdf-io/cdf-io.hpp"
#include <iostream>

std::ostream& operator<<(std::ostream& os, const cdf::Variable::shape_t& shape)
{
    os << "(";
    for (auto i = 0; i < static_cast<int>(std::size(shape)) - 1; i++)
        os << shape[i] << ',';
    if (std::size(shape) >= 1)
        os << shape[std::size(shape) - 1];
    os << ")";
    return os;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
    // cdf::io::load returns a optional<CDF>
    if (const auto my_cdf = cdf::io::load(path); my_cdf)
    {
        std::cout << "Attribute list:" << std::endl;
        for (const auto& [name, attribute] : my_cdf->attributes)
        {
            std::cout << "\t" << name << std::endl;
        }
        std::cout << "Variable list:" << std::endl;
        for (const auto& [name, variable] : my_cdf->variables)
        {
            std::cout << "\t" << name << " shape:" << variable.shape() << std::endl;
        }
        return 0;
    }
    return -1;
}
