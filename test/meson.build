gtest_dep = dependency('gtest', main : true, required : false)
if not gtest_dep.found()
    gtest_proj = import('cmake').subproject('googletest', required: false)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [
                dependency('threads'),
                gtest_proj.dependency('gtest'),
                gtest_proj.dependency('gtest_main'),
            ]
        )
    else
        assert(
            not get_option('tests').enabled(),
            'Googletest is required if tests are enabled'
            )
    endif
endif

testSourceFiles = [
        'data_sync_config_test',
    ]

foreach testFile : testSourceFiles
    test(
        'test_' + testFile.underscorify(),
        executable(
            'test-' + testFile.underscorify(),
            testFile + '.cpp',
            rbmc_data_sync_sources,
            dependencies : [
                gtest_dep,
                rbmc_data_sync_dependencies,
            ],
            include_directories: include_directories('../src'),
        )
    )
endforeach
