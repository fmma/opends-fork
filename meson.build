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

pkg = import('pkgconfig')
pkg.generate(libopengds_ref,
  name : 'opengds',
  description : 'OpenGDS reference backend',
)

# GDS backend (NVIDIA cuFile + CUDA)
cc = meson.get_compiler('c')
cuda_dep = dependency('cuda', modules : ['cuda', 'cudart'], required : false)
cufile_dep = cc.find_library('cufile',
  dirs : ['/usr/local/cuda/lib64'],
  required : false,
)

gds_enabled = cuda_dep.found() and cufile_dep.found()

if gds_enabled
  libopengds_gds = shared_library('opengds_gds',
    'src/ds_file_gds.c',
    include_directories : opengds_inc,
    dependencies : [cuda_dep, cufile_dep],
    install : true,
  )

  opengds_gds_dep = declare_dependency(
    include_directories : opengds_inc,
    link_with : libopengds_gds,
  )
endif

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

if gds_enabled
  executable('test_read_gds',
    ['tests/test_read_gds.c', read_cases_h],
    dependencies : [opengds_gds_dep, cuda_dep],
  )
endif

summary({
  'Reference backend' : true,
  'GDS backend' : gds_enabled,
}, section : 'Backends')
