#include "cdf-io/cdf-io.hpp"

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        std::cout << "reading " << argv[1];
        auto f = cdf::io::load(argv[1]);
        if (!f)
            return -1;
    }
    return 0;
}
