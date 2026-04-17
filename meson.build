project('OpenDS', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'],
)

opends_inc = include_directories('include')

# Reference backend (pread/pwrite on host buffers, no external dependencies)
libopends_ref = shared_library('opends_ref',
  'src/ds_file_common.c',
  'src/ds_file_ref.c',
  include_directories : opends_inc,
  install : true,
)

opends_ref_dep = declare_dependency(
  include_directories : opends_inc,
  link_with : libopends_ref,
)

pkg = import('pkgconfig')
pkg.generate(libopends_ref,
  name : 'opends',
  description : 'OpenDS reference backend',
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
  libopends_gds = shared_library('opends_gds',
    'src/ds_file_common.c',
    'src/ds_file_gds.c',
    include_directories : opends_inc,
    dependencies : [cuda_dep, cufile_dep],
    install : true,
  )

  opends_gds_dep = declare_dependency(
    include_directories : opends_inc,
    link_with : libopends_gds,
  )
endif

# Test binaries
executable('test_smoke_ref',
  'tests/test_smoke_ref.c',
  dependencies : [opends_ref_dep],
)

executable('test_read_ref',
  'tests/test_read_ref.c',
  dependencies : [opends_ref_dep],
)

if gds_enabled
  executable('test_read_gds',
    'tests/test_read_gds.c',
    dependencies : [opends_gds_dep, cuda_dep],
  )
else
  executable('test_read_gds', 'tests/skip.c')
endif

summary({
  'Reference backend' : true,
  'GDS backend' : gds_enabled,
}, section : 'Backends')
