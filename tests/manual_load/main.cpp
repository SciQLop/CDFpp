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
    if (argc > 1)
    {
        std::cout << "reading " << argv[1];
        auto f = cdf::io::load(std::string{argv[1]}, false, false);
        if (!f)
            return -1;
        std::cout << "Attribute list:" << std::endl;
        for (const auto& [name, attribute] : f->attributes)
        {
            std::cout << "\t" << name << std::endl;
        }
        std::cout << "Variable list:" << std::endl;
        for (const auto& [name, variable] : f->variables)
        {
            std::cout << "\t" << name << " shape:" << variable.shape() << std::endl;
        }
    }
    return 0;
}
