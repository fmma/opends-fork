project('OpenDS', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'],
)

opends_inc = include_directories('include')
opends_internal_inc = include_directories('src')

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

# aisio backend (xNVMe upcie-cuda + mock HOMI extent cache).
# The library itself only links libxnvme; CUDA context management is
# the caller's responsibility (xnvme's upcie-cuda plug-in loads CUDA
# at runtime). Tests pull in cuda_dep separately.
xnvme_dep = dependency('xnvme', required : false)
aisio_enabled = xnvme_dep.found()

if aisio_enabled
  libopends_aisio = shared_library('opends_aisio',
    'src/ds_file_common.c',
    'src/ds_file_aisio.c',
    'src/homi_client_mock.c',
    include_directories : [opends_inc, opends_internal_inc],
    dependencies : [xnvme_dep],
    install : true,
  )

  opends_aisio_dep = declare_dependency(
    include_directories : opends_inc,
    link_with : libopends_aisio,
  )
endif

# Test binaries
executable('test_smoke_ref',
  'tests/test_smoke_ref.c',
  dependencies : [opends_ref_dep],
)

# Prep binary: writes the shared pattern file consumed by every
# backend's test_sync_read_X. POSIX only, no backend linkage.
executable('test_sync_read_prep',
  'tests/test_sync_read_prep.c',
)

executable('test_sync_read_ref',
  'tests/test_sync_read_ref.c',
  dependencies : [opends_ref_dep],
)

if gds_enabled
  executable('test_sync_read_gds',
    'tests/test_sync_read_gds.c',
    dependencies : [opends_gds_dep, cuda_dep],
  )
else
  executable('test_sync_read_gds', 'tests/skip.c')
endif

aisio_test_enabled = aisio_enabled and cuda_dep.found()

if aisio_test_enabled
  executable('test_sync_read_aisio',
    'tests/test_sync_read_aisio.c',
    dependencies : [opends_aisio_dep, cuda_dep],
  )
else
  executable('test_sync_read_aisio', 'tests/skip.c')
endif

if aisio_enabled
  executable('cache_extents',
    'tools/cache_extents.c',
    include_directories : opends_internal_inc,
  )
else
  executable('cache_extents', 'tests/skip.c')
endif

summary({
  'Reference backend' : true,
  'GDS backend' : gds_enabled,
  'aisio backend' : aisio_enabled,
}, section : 'Backends')
