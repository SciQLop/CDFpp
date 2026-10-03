google_benchmarks_dep = dependency('benchmark', required : true)
# file_reader measures Linux huge pages.
benches = ['chrono', 'rle']
if host_machine.system() == 'linux'
    benches += ['file_reader']
endif
foreach bench:benches
    exe = executable('benchmark-'+bench, bench+'/main.cpp',
                    dependencies:[google_benchmarks_dep, cdfpp_dep],
                    install: false
                    )
    benchmark(bench, exe)
endforeach
