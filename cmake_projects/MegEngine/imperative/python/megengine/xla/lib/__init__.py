# code of this directory is mainly from jax: https://github.com/google/jax

try:
    import mge_xlalib as mge_xlalib
except ModuleNotFoundError as err:
    msg = (
        "mge-xla requires mge_xlalib to be installed. if this problem happened when "
        "pytest, maybe you have set --doctest-modules for pytest. you can close it "
        "by setup `norecursedirs = megengine/xla` in `pytest.ini`"
    )
    raise ModuleNotFoundError(msg)

import gc
import os
import platform
import subprocess
import sys
import warnings
from typing import Optional

import mge_xlalib.cpu_feature_guard as cpu_feature_guard
import mge_xlalib.ducc_fft as ducc_fft
import mge_xlalib.gpu_linalg as gpu_linalg
import mge_xlalib.gpu_prng as gpu_prng
import mge_xlalib.gpu_rnn as gpu_rnn
import mge_xlalib.gpu_solver as gpu_solver
import mge_xlalib.gpu_sparse as gpu_sparse
import mge_xlalib.lapack as lapack
import mge_xlalib.xla_client as xla_client

from ...core._imperative_rt.common import get_cudnn_version as _get_cudnn_version

if int(platform.python_version_tuple()[1]) < 8:
    raise RuntimeError(
        f"xla backend requires Python version >= 3.8, got {platform.python_version()}"
    )

if _get_cudnn_version() < 8600:
    warnings.warn(
        f"xla backend can get the max speed up with CUDNN version >= 8.6.0, "
        f"but current cudnn version is {_get_cudnn_version()}"
    )


cpu_feature_guard.check_cpu_features()


xla_extension = xla_client._xla
pytree = xla_client._xla.pytree
# we use some api in jaxlib
xla_jit = xla_client._xla.jax_jit
pmap_lib = xla_client._xla.pmap_lib


def _xla_gc_callback(*args):
    xla_client._xla.collect_garbage()


gc.callbacks.append(_xla_gc_callback)


xla_extension_version: int = getattr(xla_client, "_version", 0)
mlir_api_version = xla_client.mlir_api_version

# Finds the CUDA install path
def _find_cuda_root_dir() -> Optional[str]:
    assert sys.platform == "linux", "xla only support linux now"
    for envvar in ["CUDA_ROOT_DIR", "CUDA_HOME", "CUDA_PATH"]:
        if os.environ.get(envvar):
            return os.environ[envvar]

    cur_dir = os.path.dirname(os.path.abspath(__file__))
    for path in ["/usr/local/cuda", os.path.join(cur_dir, "../../../nvidia/cuda_nvcc")]:
        if os.path.exists(path):
            return path

    try:
        with open(os.devnull, "w") as devnull:
            ptxas = (
                subprocess.check_output(["which", "ptxas"], stderr=devnull)
                .decode()
                .rstrip("\r\n")
            )
            return os.path.dirname(os.path.dirname(ptxas))
    except Exception:
        assert False, "not find cuda in your environment"


cuda_path = _find_cuda_root_dir()

transfer_guard_lib = xla_client._xla.transfer_guard_lib
