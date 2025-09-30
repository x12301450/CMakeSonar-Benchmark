# -*- coding: utf-8 -*-

import functools
import os

import numpy as np

from megenginelite import *


def require_cuda(func):
    """a decorator that disables a testcase if cuda is not enabled"""

    @functools.wraps(func)
    def wrapped(*args, **kwargs):
        if LiteGlobal.get_device_count(LiteDeviceType.LITE_CUDA):
            return func(*args, **kwargs)

    return wrapped


@require_cuda
def test_tensor_collect_batch():
    batch_tensor = TensorBatchCollector(
        [4, 8, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CUDA
    )
    arr = np.ones([8, 8], "int32")
    for i in range(4):
        batch_tensor.collect(arr)
        arr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 8
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(64):
            assert data[i][j // 8][j % 8] == i + 1


def test_tensor_collect_batch_cpu():
    batch_tensor = TensorBatchCollector(
        [4, 8, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CPU
    )
    arr = np.ones([8, 8], "int32")
    for i in range(4):
        batch_tensor.collect(arr)
        arr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 8
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(64):
            assert data[i][j // 8][j % 8] == i + 1

    for i in range(4):
        t = batch_tensor.get_tensor_at(i)
        data = t.to_numpy()
        assert data.shape[0] == 1
        assert data.shape[1] == 8
        assert data.shape[2] == 8
        for j in range(64):
            assert data[0][j // 8][j % 8] == i + 1


@require_cuda
def test_tensor_collect_batch_by_index():
    batch_tensor = TensorBatchCollector(
        [4, 8, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CUDA
    )
    arr = np.ones([8, 8], "int32")
    arr += 1  # ==2
    batch_tensor.collect_id(arr, 1)
    arr -= 1  # ==1
    batch_tensor.collect_id(arr, 0)
    arr += 2  # ==3
    batch_tensor.collect_id(arr, 2)
    arr += 1  # ==4
    batch_tensor.collect_id(arr, 3)

    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 8
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(64):
            assert data[i][j // 8][j % 8] == i + 1


@require_cuda
def test_tensor_collect_batch_tensor():
    batch_tensor = TensorBatchCollector(
        [4, 6, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CUDA
    )
    nparr = np.ones([6, 8], "int32")
    tensor = LiteTensor(LiteLayout([6, 8], LiteDataType.LITE_INT))
    for i in range(4):
        tensor.set_data_by_share(nparr)
        batch_tensor.collect(tensor)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


def test_tensor_collect_batch_tensor_cpu():
    batch_tensor = TensorBatchCollector(
        [4, 6, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CPU
    )
    nparr = np.ones([6, 8], "int32")
    tensor = LiteTensor(LiteLayout([6, 8], LiteDataType.LITE_INT))
    for i in range(4):
        tensor.set_data_by_share(nparr)
        batch_tensor.collect(tensor)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


@require_cuda
def test_tensor_collect_batch_ctypes():
    batch_tensor = TensorBatchCollector(
        [4, 6, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CUDA
    )
    nparr = np.ones([6, 8], "int32")
    for i in range(4):
        in_data = nparr.ctypes.data
        batch_tensor.collect_by_ctypes(in_data, nparr.nbytes)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


def test_tensor_collect_batch_ctypes_cpu():
    batch_tensor = TensorBatchCollector(
        [4, 6, 8], dtype=LiteDataType.LITE_INT, device_type=LiteDeviceType.LITE_CPU
    )
    nparr = np.ones([6, 8], "int32")
    for i in range(4):
        in_data = nparr.ctypes.data
        batch_tensor.collect_by_ctypes(in_data, nparr.nbytes)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


@require_cuda
def test_tensor_collect_batch_device_tensor():
    all_tensor = LiteTensor(
        LiteLayout([4, 6, 8], dtype=LiteDataType.LITE_INT),
        device_type=LiteDeviceType.LITE_CUDA,
    )
    batch_tensor = TensorBatchCollector([4, 6, 8], tensor=all_tensor)
    nparr = np.ones([6, 8], "int32")
    tensor = LiteTensor(LiteLayout([6, 8], LiteDataType.LITE_INT))
    for i in range(4):
        tensor.set_data_by_share(nparr)
        batch_tensor.collect(tensor)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


@require_cuda
def test_tensor_collect_batch_device_numpy():
    all_tensor = LiteTensor(
        LiteLayout([4, 6, 8], dtype=LiteDataType.LITE_INT),
        device_type=LiteDeviceType.LITE_CUDA,
    )
    batch_tensor = TensorBatchCollector([4, 6, 8], tensor=all_tensor)
    nparr = np.ones([6, 8], "int32")
    for i in range(4):
        batch_tensor.collect(nparr)
        nparr += 1
    data = batch_tensor.to_numpy()
    assert data.shape[0] == 4
    assert data.shape[1] == 6
    assert data.shape[2] == 8
    for i in range(4):
        for j in range(48):
            assert data[i][j // 8][j % 8] == i + 1


def test_get_model_io_ahead():
    source_dir = os.getenv("LITE_TEST_RESOURCE")
    model_path = os.path.join(source_dir, "shufflenet.mge")
    ios = get_model_io_info(model_path)

    assert len(ios.inputs) == 1
    assert ios.inputs[0].name == "data"
    assert ios.inputs[0].config_layout.shapes[1] == 3
    assert ios.inputs[0].config_layout.shapes[2] == 224
    assert ios.inputs[0].config_layout.shapes[3] == 224

    assert len(ios.outputs) == 1
    assert ios.outputs[0].name == "TRUE_DIV(EXP[12065],reduce0[12067])[12077]"
    assert ios.outputs[0].config_layout.shapes[0] == 1
    assert ios.outputs[0].config_layout.shapes[1] == 1000
