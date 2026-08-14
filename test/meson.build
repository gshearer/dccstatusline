# One table-driven suite per module; suites land chunk by chunk (see TODO.md).
foreach name : ['util', 'color', 'fmt', 'json', 'payload', 'config']
  test(name, executable('test_' + name, 'test_' + name + '.c',
    include_directories: dcc_inc,
    link_with: dcccore))
endforeach
