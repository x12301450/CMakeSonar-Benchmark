# -*- coding: utf-8 -*-
import os
import pickle
from tempfile import TemporaryFile

import numpy as np

import megengine as mge
from megengine import Parameter, Tensor
from megengine.core.ops import builtin


def test_tensor_serialization():
    with TemporaryFile() as f:
        data = np.random.randint(low=0, high=7, size=[233])
        a = Tensor(data, device="cpu0", dtype=np.int32)
        mge.save(a, f)
        f.seek(0)
        b = mge.load(f)
        np.testing.assert_equal(a.numpy(), data)
        assert b.device.logical_name == "cpu0:0"
        assert b.dtype == np.int32

    with TemporaryFile() as f:
        a = Parameter(np.random.random(size=(233, 2)).astype(np.float32))
        mge.save(a, f)
        f.seek(0)
        b = mge.load(f)
        assert isinstance(b, Parameter)
        np.testing.assert_equal(a.numpy(), b.numpy())

    with TemporaryFile() as f:
        a = Tensor(np.random.random(size=(2, 233)).astype(np.float32))
        mge.save(a, f)
        f.seek(0)
        b = mge.load(f)
        assert type(b) is Tensor
        np.testing.assert_equal(a.numpy(), b.numpy())

    with TemporaryFile() as f:
        a = Tensor(np.random.random(size=(2, 233)).astype(np.float32))
        mge.save(a, f)
        f.seek(0)
        b = mge.load(f, map_location="cpux")
        assert type(b) is Tensor
        assert "cpu" in str(b.device)
        np.testing.assert_equal(a.numpy(), b.numpy())

    with TemporaryFile() as f:
        if mge.is_cuda_available():
            device_org = mge.get_default_device()
            mge.set_default_device("gpu0")
            a = Tensor(np.random.random(size=(2, 233)).astype(np.float32))
            mge.save(a, f)
            f.seek(0)
            mge.set_default_device("cpux")
            b = mge.load(f, map_location={"gpu0": "cpu0"})
            assert type(b) is Tensor
            assert "cpu0" in str(b.device)
            np.testing.assert_equal(a.numpy(), b.numpy())
            mge.set_default_device(device_org)

    with TemporaryFile() as f:
        a = Tensor(0)
        a.qparams.scale = Tensor(1.0)
        mge.save(a, f)
        f.seek(0)
        b = mge.load(f)
        assert isinstance(b.qparams.scale, Tensor)
        np.testing.assert_equal(b.qparams.scale.numpy(), 1.0)


def test_compatibility():
    def test_old_tensor(model_name):
        path = os.path.join(os.path.dirname(__file__), model_name)
        old_tensor = mge.load(path)
        assert np.all(old_tensor.numpy() == [1, 2, 3])
        assert old_tensor.device.logical_name == "cpu0:0"
        assert old_tensor.dtype == np.int8

    test_old_tensor("tensor_v1_1.mge")
    test_old_tensor("tensor_v1_2.mge")

    t = mge.tensor([1])
    getattr(t, "qparams")
    new_args = t.__getnewargs__()
    assert (
        len(new_args) == 7
        and isinstance(new_args[0], np.ndarray)
        and new_args[1] == np.int32
        and isinstance(new_args[2], str)
        and new_args[3] == False
        and new_args[4] == False
        and new_args[5] is None
        and isinstance(new_args[6], str)
    ), "Modify Tensor __getnewargs__ may break pickle serialization compatible"
    state = t.__getstate__()
    assert set(state.keys()) == set(
        ["qparams"]
    ), "Modify Tensor __getstate__ may break pickle serialization compatible"
