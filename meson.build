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

# Generated test data
python = find_program('python3')
read_cases_h = custom_target('read_cases_h',
  input : files('tests/read_cases.txt'),
  output : 'read_cases.h',
  command : [python, files('tests/gen_read_cases.py'), '@INPUT@', '@OUTPUT@'],
)

# Test binaries
executable('test_smoke_ref',
  'tests/test_smoke_ref.c',
  dependencies : [opengds_ref_dep],
)

executable('test_read_ref',
  ['tests/test_read_ref.c', read_cases_h],
  dependencies : [opengds_ref_dep],
)
