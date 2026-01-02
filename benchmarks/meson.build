google_benchmarks_dep = dependency('benchmark', required : true)
foreach bench:['file_reader', 'chrono']
    exe = executable('benchmark-'+bench, bench+'/main.cpp',
                    dependencies:[google_benchmarks_dep, cdfpp_dep],
                    install: false
                    )
    benchmark(bench, exe)
endforeach
