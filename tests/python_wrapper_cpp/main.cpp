#include <pybind11/embed.h>
namespace py = pybind11;

int main(int argc, char** argv)
{
    py::scoped_interpreter guard {};
    if (argc > 1)
    {
        py::globals()["fname"]=std::string(argv[1]);
        py::exec(R"(
            import sys
            print(sys.path)
            import pycdfpp
            cdf = pycdfpp.load(fname)
            [(print(v.type), v.as_array()) for _,v in cdf.items() if v.type is not pycdfpp.CDF_CHAR]
        )");
    }
}
