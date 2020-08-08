import os
import sys
from os.path import join, dirname
from distutils.sysconfig import get_python_inc
import subprocess
import numpy
from numpy.distutils.misc_util import get_numpy_include_dirs

try:
    from numpy.distutils.misc_util import get_info
except ImportError as e:
    raise ValueError("numpy >= 1.4 is required (detected %s from %s)" %
                     (numpy.__version__, numpy.__file__)) from e


def cxx_pre_build_hook(build_ext, ext):
    from scipy._build_utils.compiler_helper import (get_cxx_std_flag,
                                                    try_add_flag)
    cc = build_ext._cxx_compiler
    args = ext.extra_compile_args

    std_flag = get_cxx_std_flag(cc)
    if std_flag is not None:
        args.append(std_flag)
    if sys.platform == 'darwin':
        args.append('-mmacosx-version-min=10.7')
        try_add_flag(args, cc, '-stdlib=libc++')


def configuration(parent_package='',top_path=None):
    from numpy.distutils.misc_util import Configuration
    from scipy._build_utils.system_info import get_info as get_system_info
    from scipy._build_utils import combine_dict, uses_blas64

    config = Configuration('special', parent_package, top_path)

    if uses_blas64():
        lapack_opt = get_system_info('lapack_ilp64_opt')
    else:
        lapack_opt = get_system_info('lapack_opt')

    define_macros = []
    if sys.platform == 'win32':
        # define_macros.append(('NOINFINITIES',None))
        # define_macros.append(('NONANS',None))
        define_macros.append(('_USE_MATH_DEFINES',None))

    curdir = os.path.abspath(os.path.dirname(__file__))
    python_inc_dirs = get_python_inc()
    plat_specific_python_inc_dirs = get_python_inc(plat_specific=1)
    inc_dirs = [get_numpy_include_dirs(), python_inc_dirs]
    if python_inc_dirs != plat_specific_python_inc_dirs:
        inc_dirs.append(plat_specific_python_inc_dirs)
    inc_dirs.append(join(dirname(dirname(__file__)), '_lib'))
    inc_dirs.append(join(dirname(dirname(__file__)), '_build_utils', 'src'))

    # C libraries
    cephes_src = [join('cephes','*.c')]
    cephes_hdr = [join('cephes', '*.h')]
    sf_error_src = ["sf_error.c"]
    sf_error_hdr = ["sf_error.h"]
    config.add_library('sc_cephes',sources=cephes_src,
                       include_dirs=[curdir] + inc_dirs,
                       depends=(cephes_hdr + ['*.h']),
                       macros=define_macros)
    config.add_library("sf_error", sources=sf_error_src,
                       include_dirs=[curdir] + inc_dirs,
                       depends=sf_error_hdr,
                       macros=define_macros)

    # Fortran/C++ libraries
    mach_src = [join('mach','*.f')]
    amos_src = [join('amos','*.f')]
    cdf_src = [join('cdflib','*.f')]
    specfun_src = [join('specfun','*.f')]
    config.add_library('sc_mach',sources=mach_src,
                       config_fc={'noopt':(__file__,1)})
    config.add_library('sc_amos',sources=amos_src)
    config.add_library('sc_cdf',sources=cdf_src)
    config.add_library('sc_specfun',sources=specfun_src)

    # Extension specfun
    config.add_extension('specfun',
                         sources=['specfun.pyf'],
                         f2py_options=['--no-wrap-functions'],
                         depends=specfun_src,
                         define_macros=[],
                         libraries=['sc_specfun'])

    # Extension _ufuncs
    headers = ['*.h', join('cephes', '*.h')]
    ufuncs_src = ['_ufuncs.c', '_logit.c.src',
                  "amos_wrappers.c", "cdf_wrappers.c", "specfun_wrappers.c"]
    ufuncs_dep = (
        headers
        + ufuncs_src
        + amos_src
        + cephes_src
        + mach_src
        + cdf_src
        + specfun_src
        + sf_error_src
        + sf_error_hdr
    )
    cfg = combine_dict(lapack_opt,
                       include_dirs=[curdir] + inc_dirs + [numpy.get_include()],
                       libraries=['sc_amos', 'sc_cephes', 'sc_mach',
                                  'sc_cdf', 'sc_specfun', 'sf_error'],
                       define_macros=define_macros)
    config.add_extension('_ufuncs',
                         depends=ufuncs_dep,
                         sources=ufuncs_src,
                         extra_info=get_info("npymath"),
                         **cfg)

    # Extension _ufuncs_cxx
    ufuncs_cxx_src = ['_ufuncs_cxx.cxx',
                      'ellint_carlson_wrap.cxx',
                      '_faddeeva.cxx', 'Faddeeva.cc',
                      '_wright.cxx', 'wright.cc']
    ufuncs_cxx_dep = (headers + ufuncs_cxx_src + cephes_src +
                      ['*.hh', join('ellint_carlson_cpp_lite', '*.hh')] +
                      sf_error_src + sf_error_hdr)
    ext_cxx = config.add_extension('_ufuncs_cxx',
                                   sources=ufuncs_cxx_src,
                                   depends=ufuncs_cxx_dep,
                                   include_dirs=[curdir] + inc_dirs,
                                   define_macros=define_macros,
                                   libraries=["sf_error"],
                                   extra_info=get_info("npymath"))
    ext_cxx._pre_build_hook = cxx_pre_build_hook

    cfg = combine_dict(lapack_opt, include_dirs=inc_dirs)
    cfg.setdefault('libraries', []).extend(["sf_error"])
    config.add_extension('_ellip_harm_2',
                         sources=['_ellip_harm_2.c'],
                         depends=sf_error_src + sf_error_hdr,
                         **cfg)

    # Cython API
    config.add_data_files('cython_special.pxd')

    cython_special_src = ['cython_special.c', 'sf_error.c', '_logit.c.src',
                          "amos_wrappers.c", "cdf_wrappers.c", "specfun_wrappers.c"]
    cython_special_dep = (
        headers
        + ufuncs_src
        + ufuncs_cxx_src
        + amos_src
        + cephes_src
        + mach_src
        + cdf_src
        + specfun_src
    )
    cfg = combine_dict(lapack_opt,
                       include_dirs=[curdir] + inc_dirs + [numpy.get_include()],
                       libraries=['sc_amos', 'sc_cephes', 'sc_mach',
                                  'sc_cdf', 'sc_specfun', 'sf_error'],
                       define_macros=define_macros)
    config.add_extension('cython_special',
                         depends=cython_special_dep,
                         sources=cython_special_src,
                         extra_info=get_info("npymath"),
                         **cfg)

    # combinatorics
    config.add_extension('_comb',
                         sources=['_comb.c'])

    # testing for _round.h
    config.add_extension('_test_round',
                         sources=['_test_round.c'],
                         depends=['_round.h', 'cephes/dd_idefs.h'],
                         include_dirs=[numpy.get_include()] + inc_dirs,
                         extra_info=get_info('npymath'))

    config.add_data_files('tests/*.py')
    config.add_data_files('tests/data/README')

    # regenerate npz data files
    makenpz = os.path.join(os.path.dirname(__file__),
                           'utils', 'makenpz.py')
    data_dir = os.path.join(os.path.dirname(__file__),
                            'tests', 'data')
    for name in ['boost', 'gsl', 'local']:
        subprocess.check_call([sys.executable, makenpz,
                               '--use-timestamp',
                               os.path.join(data_dir, name)])

    config.add_data_files('tests/data/*.npz')

    config.add_subpackage('_precompute')

    # Type stubs
    config.add_data_files('*.pyi')

    return config


if __name__ == '__main__':
    from numpy.distutils.core import setup
    setup(**configuration(top_path='').todict())
