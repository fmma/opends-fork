project('OpenGDS', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'],
)

opengds_inc = include_directories('include')

# Reference backend (pread/pwrite on host buffers, no external dependencies)
libopengds_ref = shared_library('opengds_ref',
  'src/ds_file_ref.c',
  include_directories : opengds_inc,
  install : true,
)

opengds_ref_dep = declare_dependency(
  include_directories : opengds_inc,
  link_with : libopengds_ref,
)

# Test binaries
executable('smoke_ref',
  'tests/smoke_ref.c',
  dependencies : [opengds_ref_dep],
)
