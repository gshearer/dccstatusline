project('dccstatusline', 'c',
  version: '0.0.3',
  license: 'MIT',
  meson_version: '>=1.4.0',
  default_options: [
    'c_std=gnu23',
    'warning_level=3',
    'werror=true',
    'buildtype=debugoptimized',
  ])

cc = meson.get_compiler('c')

add_project_arguments(
  '-Wvla',
  '-Wshadow',
  '-Wformat=2',
  language: 'c')

# Hardening flags are feature-detected, because not every toolchain accepts
# them: Apple clang recognizes -fstack-clash-protection but marks it unused,
# which werror=true turns fatal.
#
# Detection must never mean silent degradation, though. On Linux — the platform
# the release binaries target, and the one whose hardening BUILD.md makes
# promises about — a flag the compiler will not honor is a configure-time error,
# not a quiet downgrade discovered later by reading a build log nobody kept.
hardening = ['-fstack-protector-strong', '-fstack-clash-protection']
honored = cc.get_supported_arguments(hardening)

if host_machine.system() == 'linux'
  missing = []

  foreach flag : hardening
    if not honored.contains(flag)
      missing += flag
    endif
  endforeach

  if missing.length() > 0
    error('@0@ will not honor required hardening: @1@'.format(
      cc.get_id(), ', '.join(missing)))
  endif
endif

add_project_arguments(honored, language: 'c')

# _FORTIFY_SOURCE needs optimization and fights sanitizer interceptors, so it is
# applied per-configuration rather than globally. -U guards against distro
# toolchains that predefine =2.
# b_sanitize is a string in current meson but becomes a list in newer ones;
# formatting normalizes both so the comparison can never type-error.
san = '@0@'.format(get_option('b_sanitize'))
sanitized = san != 'none' and san != '[]'

if not ['plain', '0', 'g'].contains(get_option('optimization')) and not sanitized
  add_project_arguments('-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=3', language: 'c')
endif

# Fuzz builds instrument everything with libFuzzer coverage; the harnesses
# themselves add the driver via -fsanitize=fuzzer in test/meson.build.
if get_option('fuzz')
  add_project_arguments('-fsanitize=fuzzer-no-link', language: 'c')
  add_project_link_arguments('-fsanitize=fuzzer-no-link', language: 'c')
endif

buildnum = executable('buildnum', 'tools' / 'buildnum.c', native: true)

subdir('src')
subdir('test')
