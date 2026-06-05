project('OpenDS', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'],
)

opends_inc = include_directories('include')
opends_internal_inc = include_directories('src')
tests_inc = include_directories('tests')

public_headers = files(
  'include/ds_file.h',
  'include/ds_file_async.h',
  'include/ds_file_batch.h',
  'include/opends.h',
)
install_headers(public_headers)

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

# aisio backend (xNVMe upcie-cuda + HOMI-served qpairs and extents).
# Links libxnvme and the CUDA driver API. The driver API is needed for
# stream-async ds_file_read_async, which gates the user's stream with
# cuStreamWriteValue32 / cuStreamWaitValue32 against host pinned mapped
# trigger and completion words polled by an internal I/O thread.
xnvme_dep = dependency('xnvme', required : false)
homic_dep = dependency('homic', required : false)
cuda_drv_dep = cc.find_library('cuda',
  dirs : [
    '/usr/local/cuda/lib64/stubs',
    '/usr/lib/x86_64-linux-gnu/stubs',
    '/usr/local/cuda/lib64',
  ],
  required : false,
)

# cuda.h ships with the CUDA toolkit (under .../targets/<arch>/include
# on Ubuntu) but a minimal driver-dev package may put it on the system
# include path instead. Probe toolkit locations first; fall back to a
# system-wide cc.has_header check.
cuda_drv_inc_dirs = []
foreach p : [
  '/usr/local/cuda/targets/x86_64-linux/include',
  '/usr/local/cuda/include',
]
  if cuda_drv_inc_dirs.length() == 0 and \
      run_command('test', '-f', p / 'cuda.h', check : false).returncode() == 0
    cuda_drv_inc_dirs += p
  endif
endforeach
have_cuda_h = cuda_drv_inc_dirs.length() > 0 or cc.has_header('cuda.h')

aisio_enabled = xnvme_dep.found() and cuda_dep.found() and \
                cuda_drv_dep.found() and have_cuda_h and homic_dep.found()

if aisio_enabled
  # The async path links a tiny D2D copy kernel compiled from
  # src/ds_bounce_kernel.cu. Adding the cuda language only when aisio is
  # actually built keeps ref-only builds free of an nvcc dependency.
  add_languages('cuda', required : true, native : false)

  aisio_inc_dirs = [opends_inc, opends_internal_inc]
  if cuda_drv_inc_dirs.length() > 0
    aisio_inc_dirs += include_directories(cuda_drv_inc_dirs,
                                          is_system : true)
  endif

  libopends_aisio = shared_library('opends_aisio',
    'src/ds_file_common.c',
    'src/ds_file_aisio.c',
    'src/ds_bounce_kernel.cu',
    'src/ds_bounce_ctx.c',
    include_directories : aisio_inc_dirs,
    dependencies : [xnvme_dep, cuda_dep, cuda_drv_dep, homic_dep],
    install : true,
  )

  opends_aisio_dep = declare_dependency(
    include_directories : opends_inc,
    link_with : libopends_aisio,
  )

  # Consumer-facing header: the file-to-LBA extent record external
  # benchmarks deserialize from the test-prep tool.
  install_headers(
    'src/ds_extent.h',
  )

  pkg.generate(libopends_aisio,
    name : 'opends_aisio',
    description : 'OpenDS aisio backend (raw NVMe via xNVMe upcie-cuda)',
    requires : ['xnvme'],
  )
endif

# Test binaries
executable('test_smoke_ref',
  'tests/test_smoke_ref.c',
  dependencies : [opends_ref_dep],
)

# Prep binary: writes the shared pattern file consumed by every
# backend's test_sync_read_X, and the small extents record consumed by
# the aisio leg (which runs after the FS is unmounted).
executable('test_sync_read_prep',
  'tests/test_sync_read_prep.c',
  'tests/test_extents_io.c',
  include_directories : [opends_internal_inc, tests_inc],
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
  aisio_test_inc_dirs = [opends_internal_inc, tests_inc]
  if cuda_drv_inc_dirs.length() > 0
    aisio_test_inc_dirs += include_directories(cuda_drv_inc_dirs,
                                               is_system : true)
  endif

  executable('test_sync_read_aisio',
    'tests/test_sync_read_aisio.c',
    include_directories : aisio_test_inc_dirs,
    dependencies : [opends_aisio_dep, cuda_dep],
  )
  executable('test_register_large_aisio',
    'tests/test_register_large_aisio.c',
    include_directories : aisio_test_inc_dirs,
    dependencies : [opends_aisio_dep, cuda_dep],
  )
  executable('test_async_read_aisio',
    'tests/test_async_read_aisio.c',
    include_directories : aisio_test_inc_dirs,
    dependencies : [opends_aisio_dep, cuda_dep, cuda_drv_dep],
  )
else
  executable('test_sync_read_aisio', 'tests/skip.c')
  executable('test_register_large_aisio', 'tests/skip.c')
  executable('test_async_read_aisio', 'tests/skip.c')
endif

summary({
  'Reference backend' : true,
  'GDS backend' : gds_enabled,
  'aisio backend' : aisio_enabled,
}, section : 'Backends')
