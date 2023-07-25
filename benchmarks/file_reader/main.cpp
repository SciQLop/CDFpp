#include <benchmark/benchmark.h>
#include <cdfpp/no_init_vector.hpp>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/mman.h> // madvise

inline constexpr std::size_t mega(std::size_t n)
{
    return n * 1024 * 1024;
}


auto make_test_file(std::size_t count)
{
    /*auto path = std::filesystem::temp_directory_path()
        /= std::filesystem::path { "pycdfpp_benchmark.bin" };*/
    auto path = std::filesystem::path { "/home/jeandet/pycdfpp_benchmark.bin" };
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove(path);
    }
    auto os = std::fstream { path, std::ios::out | std::ios::binary };
    for (auto i = 0UL; i < count; i++)
    {
        os << static_cast<uint64_t>(i);
    }
    os.flush();
    os.close();
    return path;
}


static void BM_raw_ifstream_read_std_vector_and_madvise(benchmark::State& state)
{
    auto test_file = make_test_file(state.range(0));
    auto size = std::filesystem::file_size(test_file);
    std::vector<char> data(size);
    madvise(data.data(), size, MADV_HUGEPAGE);
    for (auto _ : state)
    {
        auto ifs = std::fstream { test_file, std::ios::in | std::ios::binary };
        ifs.read(reinterpret_cast<char*>(data.data()), size);
    }
    state.counters["Bytes"] = size;
    state.counters["Read Speed"] = benchmark::Counter(
        size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}
BENCHMARK(BM_raw_ifstream_read_std_vector_and_madvise)
    ->Name("raw_ifstream_read with std::vector and madvise")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();



template <class TT>
static void BM_raw_ifstream_read(benchmark::State& state)
{
    auto test_file = make_test_file(state.range(0));
    auto size = std::filesystem::file_size(test_file);
    TT data(size);
    for (auto _ : state)
    {
        auto ifs = std::fstream { test_file, std::ios::in | std::ios::binary };
        ifs.read(reinterpret_cast<char*>(data.data()), size);
    }
    state.counters["Bytes"] = size;
    state.counters["Read Speed"] = benchmark::Counter(
        size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}
BENCHMARK(BM_raw_ifstream_read<std::vector<char>>)
    ->Name("raw_ifstream_read with std::vector")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_ifstream_read<no_init_vector<char>>)
    ->Name("raw_ifstream_read with no_init_vector(backed with huge pages)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();

#if __has_include(<sys/mman.h>)
#include <linux/mman.h>
#include <sys/mman.h>
#include <unistd.h>
template <bool use_madvise = false, bool use_huge_pages = false>
static void BM_raw_mmap_read(benchmark::State& state)
{
    auto test_file = make_test_file(state.range(0));
    auto size = std::filesystem::file_size(test_file);
    char* data;
    posix_memalign(reinterpret_cast<void**>(&data), 1 << 21, size);
    madvise(data, size, MADV_HUGEPAGE);
    for (auto _ : state)
    {
        if (auto fd = open(test_file.c_str(), O_RDONLY, static_cast<mode_t>(0600)); fd != -1)
        {
            auto mapped_file = mmap(nullptr, size, PROT_READ,
                MAP_FILE | MAP_PRIVATE | (use_huge_pages ? 0 : 0), fd, 0UL);
            if constexpr (use_madvise)
                madvise(mapped_file, size, MADV_SEQUENTIAL);
            if (mapped_file)
            {
                std::memcpy(data, reinterpret_cast<char*>(mapped_file), size);
                munmap(mapped_file, size);
            }
            close(fd);
        }
    }
    free(data);
    state.counters["Bytes"] = size;
    state.counters["Read Speed"] = benchmark::Counter(
        size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}
BENCHMARK(BM_raw_mmap_read<false, false>)
    ->Name("mmap madvise(off) huge pages(off)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_mmap_read<true, false>)
    ->Name("mmap madvise(on) huge pages(off)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_mmap_read<false, true>)
    ->Name("mmap madvise(off) huge pages(on)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_mmap_read<true, true>)
    ->Name("mmap madvise(on) huge pages(on)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
#endif

template <std::size_t fread_buffer, bool do_advise>
static void BM_raw_fread(benchmark::State& state)
{
    auto test_file = make_test_file(state.range(0));
    auto size = std::filesystem::file_size(test_file);
    char* data;
    posix_memalign(reinterpret_cast<void**>(&data), 1 << 21, size);
    if constexpr (do_advise)
        madvise(data, size, MADV_HUGEPAGE);
    for (auto _ : state)
    {
        auto fd = ::fopen(test_file.c_str(), "rb");
        if constexpr (fread_buffer > 0)
            setvbuf(fd, NULL, _IOFBF, fread_buffer);
        fread(data, size, 1, fd);
        ::fclose(fd);
    }
    free(data);
    state.counters["Bytes"] = size;
    state.counters["Read Speed"] = benchmark::Counter(
        size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}
BENCHMARK(BM_raw_fread<1024 * 1024 * 32, true>)
    ->Name("Raw fread with 32M cache (madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<1024 * 32, true>)
    ->Name("Raw fread with 32k cache (madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<1024 * 512, true>)
    ->Name("Raw fread with 512k cache (madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<0, true>)
    ->Name("Raw fread without cache (madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();

BENCHMARK(BM_raw_fread<1024 * 1024 * 32, false>)
    ->Name("Raw fread with 32M cache (NO madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<1024 * 32, false>)
    ->Name("Raw fread with 32k cache (NO madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<1024 * 512, false>)
    ->Name("Raw fread with 512k cache (NO madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_fread<0, false>)
    ->Name("Raw fread without cache (NO madvise)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();


template <class TT>
static void BM_raw_read(benchmark::State& state)
{
    auto test_file = make_test_file(state.range(0));
    auto size = std::filesystem::file_size(test_file);
    TT data(size);
    for (auto _ : state)
    {
        auto fd = ::open(test_file.c_str(), O_RDONLY);
        if (fd != -1)
        {
            auto r = read(fd, reinterpret_cast<char*>(data.data()), size);
            ::close(fd);
            if (static_cast<std::size_t>(r) != size)
            {
                throw std::runtime_error { "didn't read all data :/" };
            }
        }
    }
    state.counters["Bytes"] = size;
    state.counters["Read Speed"] = benchmark::Counter(
        size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}
BENCHMARK(BM_raw_read<std::vector<char>>)
    ->Name("with std::vector")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();
BENCHMARK(BM_raw_read<no_init_vector<char>>)
    ->Name("with no_init_vector(backed with huge pages)")
    ->RangeMultiplier(4)
    ->Range(mega(4), mega(64))
    ->Complexity();

BENCHMARK_MAIN();
