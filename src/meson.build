# version.h regenerates on every ninja invocation (that is the build-number
# contract). Only main.c includes it, so the cost is one tiny TU and a relink.
version_h = custom_target('version.h',
  output: 'version.h',
  command: [
    buildnum,
    '@OUTPUT@',
    meson.project_source_root() / 'build_number.txt',
    meson.project_version(),
  ],
  build_always_stale: true)

dcc_inc = include_directories('.')

dcccore = static_library('dcccore',
  'color.c',
  'fmt.c',
  'util.c')

dccstatusline = executable('dccstatusline',
  'main.c',
  version_h,
  link_with: dcccore,
  install: true)
