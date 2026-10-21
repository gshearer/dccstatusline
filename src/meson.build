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

# The reference config, carried in the binary so `--print-config` documents the
# dialect of the very build that will read it — never a newer or older copy.
reference_h = custom_target('reference_config.h',
  input: meson.project_source_root() / 'examples' / 'config',
  output: 'reference_config.h',
  command: [embed, '@OUTPUT@', '@INPUT@', 'reference_config'])

dcc_inc = include_directories('.')

dcccore = static_library('dcccore',
  'color.c',
  'config.c',
  'fmt.c',
  'gitinfo.c',
  'json.c',
  'payload.c',
  'sections.c',
  'util.c')

dccstatusline = executable('dccstatusline',
  'main.c',
  version_h,
  reference_h,
  link_with: dcccore,
  install: true)
