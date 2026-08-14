# One table-driven suite per module.
foreach name : ['util', 'color', 'fmt', 'json', 'payload', 'config',
                'gitinfo', 'sections']
  test(name, executable('test_' + name, 'test_' + name + '.c',
    include_directories: dcc_inc,
    link_with: dcccore))
endforeach

# End to end: fixture payloads through the real binary.
test('cli', executable('test_cli', 'test_cli.c'),
  args: [dccstatusline])
