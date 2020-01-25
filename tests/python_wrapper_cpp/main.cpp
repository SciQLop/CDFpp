#include <pybind11/embed.h> // everything needed for embedding
namespace py = pybind11;

int main() {
    py::scoped_interpreter guard{};
    py::exec(R"(
            import pycdfpp
            cdf = pycdfpp.load("")
        )");
}
