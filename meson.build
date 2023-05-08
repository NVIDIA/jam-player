project('jam-player', 'c',
        version: '0.1', meson_version: '>=0.57.0',
        default_options: [
          'warning_level=3',
          'werror=true',
          'b_lto=true',
        ])


# Common configurations for src and test
cdata = configuration_data()

compiler_args = ['-DPORT=OPENBMC_AST', '-Wno-error=implicit-fallthrough']

subdir('source')
