#include <cdfpp/cdf.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#if __has_include(<emscripten.h>)
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_EXPORT(x) x
#endif


EMSCRIPTEN_KEEPALIVE
emscripten::val count_variables(std::string url)
{
    struct fetch_data
    {
        bool done = false;
        std::vector<char> data;
    };
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = new fetch_data;
    attr.onsuccess = [](emscripten_fetch_t* fetch)
    {
        auto data = static_cast<fetch_data*>(fetch->userData);
        data->data = std::vector<char>(fetch->data, fetch->data + fetch->numBytes);
        data->done = true;
        emscripten_fetch_close(fetch);
    };
    attr.onerror = [](emscripten_fetch_t* fetch)
    {
        auto data = static_cast<fetch_data*>(fetch->userData);
        data->done = true;
        emscripten_fetch_close(fetch);
    };
    emscripten_fetch(&attr, url.c_str());
    while (!static_cast<fetch_data*>(attr.userData)->done)
    {
        emscripten_sleep(10);
    }
    auto data = static_cast<fetch_data*>(attr.userData);
    if (auto cdf_file = cdf::io::load(data->data); cdf_file)
    {
        std::cout << "Variables: " << std::size(cdf_file->variables) << "\n";
        auto sz = std::size(cdf_file->variables);
        delete data;
        return emscripten::val(sz);
    }
    return emscripten::val(-1);
}


EMSCRIPTEN_KEEPALIVE
int my_cdf_test()
{
    return 42;
}


#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(cdfppjs)
{
    emscripten::function("my_cdf_test", &my_cdf_test);
    emscripten::function("count_variables", &count_variables);
}
#endif
