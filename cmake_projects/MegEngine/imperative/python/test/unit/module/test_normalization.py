# -*- coding: utf-8 -*-
import numpy as np

import megengine.module.normalization as norm
from megengine import tensor


def shape_to_tuple(shape):
    if isinstance(shape, tensor):
        shape = tuple(shape.tolist())
    return shape


def test_group_norm():
    input_shape = (2, 100, 128, 128)
    channels = input_shape[1]
    groups = [2, 5, 10, 50]
    x = tensor(np.random.rand(*input_shape))
    for group in groups:
        gn = norm.GroupNorm(group, channels)
        out = gn(x)
        assert shape_to_tuple(out.shape) == input_shape


def test_layer_norm():
    input_shape_list = [(2, 3, 10, 10), (2, 2, 3, 10, 10)]
    ln = norm.LayerNorm((10, 10))
    for input_shape in input_shape_list:
        x = tensor(np.random.rand(*input_shape))
        out = ln(x)
        assert shape_to_tuple(out.shape) == input_shape


def test_instance_norm():
    input_shape = (2, 100, 128, 128)
    channels = input_shape[1]
    x = tensor(np.random.rand(*input_shape))
    inst_norm = norm.InstanceNorm(channels)
    out = inst_norm(x)
    assert shape_to_tuple(out.shape) == input_shape
