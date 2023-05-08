configure_file(output: 'config.h',
            configuration: cdata, 
)
src_inc = include_directories('.')

source_files = [
  'jamarray.c',
  'jamcomp.c',
  'jamcrc.c',
  'jamexec.c',
  'jamexp.c',
  'jamheap.c',
  'jamjtag.c',
  'jamnote.c',
  'jamstack.c',
  'jamstub.c',
  'jamsym.c',
  'jamutil.c',
]

executable('jam-player',
            sources: source_files,
            include_directories: src_inc,
            c_args: compiler_args,
            install: true,
            install_dir: get_option('bindir')
)
