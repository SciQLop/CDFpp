#include "cdfpp/attribute.hpp"
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
        if (auto maybe_cdf = cdf::io::load(std::string { argv[1] }, false, false); maybe_cdf)
        {
            auto cdf = *maybe_cdf;
            cdf.compression = cdf::cdf_compression_type::gzip_compression;
            cdf::io::save(cdf, "/tmp/test.cdf");
            if (auto res = cdf::io::save(cdf); std::size(res))
                std::cout << "success!\n";
            else
                std::cout << "failed!\n";
        }
    }
    return 0;
}
