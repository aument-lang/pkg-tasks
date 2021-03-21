project('pkg-tasks', 'c')

cc = meson.get_compiler('c')

aument = find_program('aument')
add_project_arguments(
    run_command(aument, [
        '--cflags'
    ]).stdout(),
    language: ['c']
)
add_global_link_arguments(
    run_command(aument, [
        '--libs'
    ]).stdout(),
    language: ['c']
)

uv_dep = cc.find_library('uv')

src = [
    'src/tasks.c',
    'src/tasks.h',
    'src/timer.c',
    'src/timer.h',
    'src/file.c',
    'src/file.h',
]
shared_module('tasks', src,
    include_directories: include_directories('src'),
    dependencies: [uv_dep])