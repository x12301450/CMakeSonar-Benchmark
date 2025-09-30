# -*- coding: utf-8 -*-
from typing import Iterable, Optional, Tuple, Union

import numpy as np

from ..core import _config
from ..core._imperative_rt.core2 import apply
from ..core.ops import builtin
from ..core.tensor import megbrain_graph, utils
from ..core.tensor.utils import astensor1d
from ..tensor import Tensor
from .elemwise import cos, floor, sin
from .math import argsort
from .tensor import broadcast_to, concat, expand_dims, reshape, transpose, zeros

__all__ = [
    "correlation",
    "cvt_color",
    "interpolate",
    "nms",
    "nvof",
    "remap",
    "roi_align",
    "roi_pooling",
    "warp_affine",
    "warp_perspective",
    "flip",
    "rotate",
    "rot90",
    "resize",
]


def cvt_color(inp: Tensor, mode: str = ""):
    r"""Convert images from one format to another

    Args:
        inp: input images.
        mode: format mode.

    Returns:
        convert result.

    Note:
        There are different supported modes for different combinations of :attr:`~.Tensor.device` and :attr:`~.Tensor.dtype`.

        x86/ARM:

        float32:
        "RGB2GRAY", "RGB2YUV", "YUV2RGB", "GRAY2RGB", "BGR2GRAY"

        uint8:
        "RGB2GRAY", "RGB2YUV", "YUV2RGB", "GRAY2RGB", "RGBA2RGB", "RGBA2BGR",
        "RGBA2GRAY", "RGB2BGR", "BGR2GRAY", "BGR2RGB", "YUV2GRAY_NV21", "YUV2RGB_NV21",
        "YUV2BGR_NV21", "YUV2GRAY_NV12", "YUV2RGB_NV12", "YUV2BGR_NV12", "YUV2GRAY_YV12",
        "YUV2RGB_YV12", "YUV2BGR_YV12", "YUV2GRAY_YU12", "YUV2RGB_YU12", "YUV2BGR_YU12",
        "YCrCb2RGB", "YCrCb2BGR", "BT601_YUV2RGB_NV21", "BT601_YUV2BGR_NV21", "BT601_YUV2RGB_NV12",
        "BT601_YUV2BGR_NV12", "BT601_YUV2RGB_YV12", "BT601_YUV2BGR_YV12" ,"BT601_YUV2RGB_YU12",
        "BT601_YUV2BGR_YU12"


        CUDA:

        float32:
        "RGB2GRAY", "BGR2GRAY", "RGB2YUV", "YUV2RGB", "GRAY2RGB"

        uint8:
        "RGB2GRAY", "BGR2GRAY", "RGB2YUV", "YUV2RGB", "GRAY2RGB",
        "YUV2GRAY_NV12",  "YUV2GRAY_NV21", "YUV2GRAY_YU12"
        "YUV2GRAY_YV12", "YUV2RGB_NV12", "YUV2RGB_NV21", "YUV2BGR_NV12"
        "YUV2BGR_NV21", "YUV2RGB_YU12", "YUV2RGB_YV12", "YUV2BGR_YU12",
        "YUV2BGR_YV12"


    Examples:
        >>> import numpy as np
        >>> x = mge.tensor(np.array([[[[-0.58675045, 1.7526233, 0.10702174]]]]).astype(np.float32))
        >>> y = F.vision.cvt_color(x, mode="RGB2GRAY")
        >>> y.numpy()
        array([[[[0.86555195]]]], dtype=float32)
    """
    mode = mode.upper() if "YCrCb" not in mode else mode
    assert mode in builtin.CvtColor.Mode.__dict__, "unspport mode for cvt_color"
    mode = getattr(builtin.CvtColor.Mode, mode)
    assert isinstance(mode, builtin.CvtColor.Mode)
    op = builtin.CvtColor(mode=mode)
    (out,) = apply(op, inp)
    return out


def roi_pooling(
    inp: Tensor,
    rois: Tensor,
    output_shape: Union[int, tuple, list],
    mode: str = "max",
    scale: float = 1.0,
) -> Tensor:
    r"""Applies RoI (Region of Interest) pooling on input feature, as described in Faster RCNN.

    .. seealso::

        * `Region of interest pooling explained <https://deepsense.ai/region-of-interest-pooling-explained/>`_
        * `Faster R-CNN <https://arxiv.org/abs/1506.01497>`_

    Args:
        inp: the input tensor that represents the input feature with ``(n, c, h, w)`` shape.
        rois: a tensor represents Regions of Interest with shape ``(K, 5)``, which means total ``K`` box coordinates in ``(idx, x1, y1, x2, y2)`` format where the regions will be taken from.
            The coordinate including ``(x1, y1)`` and ``(x2, y2)`` must satisfy ``0 <= x1 < x2`` and ``0 <= y1 < y2``.
            The first column ``idx`` should contain the index of the corresponding element in the input batch, i.e. a number in ``[0, n - 1]``.
        mode: "max" or "average", the pooling mode to be used. Default: "max"
        scale: It is a scale that maps output rois feature to input feature. For example, if the output is 224 * 224 image, and the input is a 112 * 112 feature map, then the scale should be set to 0.5. The default value is 1.0

    Returns:
        output tensor. ``(K, C, output_shape[0], output_shape[1])`` feature of rois.

    Examples:
        >>> import numpy as np
        >>> np.random.seed(42)
        >>> inp = Tensor(np.random.randn(1, 1, 128, 128))
        >>> rois = Tensor(np.random.random((4, 5)))
        >>> y = F.vision.roi_pooling(inp, rois, (2, 2))
        >>> y.numpy()[0].round(decimals=4)
        array([[[-0.1383, -0.1383],
                [-0.5035, -0.5035]]], dtype=float32)
    """
    assert mode.lower() in ["max", "average"], "only max/average mode is supported"
    if isinstance(output_shape, int):
        output_shape = (output_shape, output_shape)

    op = builtin.ROIPooling(mode=mode, scale=scale)
    result, _ = apply(
        op, inp, rois, Tensor(output_shape, dtype="int32", device=inp.device)
    )
    return result


def correlation(
    data1: Tensor,
    data2: Tensor,
    kernel_size: int = 1,
    max_displacement: int = 1,
    stride1: int = 1,
    stride2: int = 1,
    pad_size: int = 0,
    is_multiply: bool = True,
) -> Tensor:
    r"""Applies correlation to inputs.

    Args:
        data1: Input data1 to the correlation. format must be nchw
        data2: Input data2 to the correlation. format must be nchw
        kernel_size: int (non-negative), optional, default=1) – kernel size for Correlation must be an odd number
        max_displacement: int (non-negative), optional, default=1) – Max displacement of Correlation
        stride1: int (non-negative), optional, default=1) – stride1 quantize data1 globally
        stride2: int (non-negative), optional, default=1) – stride2 quantize data2 within the neighborhood centered around data1
        pad_size: int (non-negative), optional, default=0) – pad for Correlation
        is_multiply: boolean, optional, default=True) – operation type is either multiplication or absolute difference
    """
    # Currently correlation only support NCHW mode
    format = "NCHW"

    op = builtin.Correlation(
        format=format,
        kernel_size=kernel_size,
        max_displacement=max_displacement,
        stride1=stride1,
        stride2=stride2,
        pad_size=pad_size,
        is_multiply=is_multiply,
    )

    result, *_ = apply(op, data1, data2)
    return result


def roi_align(
    inp: Tensor,
    rois: Tensor,
    output_shape: Union[int, tuple, list],
    mode: str = "average",
    spatial_scale: float = 1.0,
    sample_points: Union[int, tuple, list] = 2,
    aligned: bool = True,
) -> Tensor:
    r"""Applies RoI (Region of Interest) align on input feature, as described in Mask R-CNN.

    .. seealso::

        * `RoIAlign <https://paperswithcode.com/method/roi-align>`_
        * `Mask R-CNN <https://arxiv.org/abs/1703.06870v3>`_

    Args:
        inp: the input tensor that represents the input feature with ``(n, c, h, w)`` shape.
        rois: a tensor represents Regions of Interest with shape ``(K, 5)``, which means total ``K`` box coordinates in ``(idx, x1, y1, x2, y2)`` format where the regions will be taken from.
            The coordinate including ``(x1, y1)`` and ``(x2, y2)`` must satisfy ``0 <= x1 < x2`` and ``0 <= y1 < y2``.
            The first column ``idx`` should contain the index of the corresponding element in the input batch, i.e. a number in ``[0, n - 1]``.
        output_shape: ``(height, width)`` shape of output rois feature.
        mode: "max" or "average", use max/average align just like max/average pooling. Default: "average"
        spatial_scale: scale the input boxes by this number. Default: 1.0
        sample_points: number of inputs samples to take for each output sample.
            0 to take samples densely. Default: 2
        aligned: wheather to align the input feature, with ``aligned=True``,
            we first appropriately scale the ROI and then shift it by -0.5. Default: True

    Returns:
        output tensor.

    Examples:
        >>> import numpy as np
        >>> np.random.seed(42)
        >>> inp = Tensor(np.random.randn(1, 1, 128, 128))
        >>> rois = Tensor(np.random.random((4, 5)))
        >>> y = F.vision.roi_align(inp, rois, (2, 2))
        >>> y.numpy()[0].round(decimals=4)
        array([[[0.175 , 0.175 ],
                [0.1359, 0.1359]]], dtype=float32)
    """
    if inp.dtype != np.float32:
        inp = inp.astype(np.float32)
    mode = mode.lower()
    assert mode in ["max", "average"], "only max/average mode is supported"
    if isinstance(output_shape, int):
        output_shape = (output_shape, output_shape)
    pooled_height, pooled_width = output_shape
    if isinstance(sample_points, int):
        sample_points = (sample_points, sample_points)
    sample_height, sample_width = sample_points
    offset = 0.5 if aligned else 0.0

    # Currently roi_align only support NCHW mode
    format = "NCHW"

    op = builtin.ROIAlign(
        mode=mode,
        format=format,
        spatial_scale=spatial_scale,
        offset=offset,
        pooled_height=pooled_height,
        pooled_width=pooled_width,
        sample_height=sample_height,
        sample_width=sample_width,
    )
    result, *_ = apply(op, inp, rois)
    return result


def nms(
    boxes: Tensor, scores: Tensor, iou_thresh: float, max_output: Optional[int] = None
) -> Tensor:
    r"""Performs non-maximum suppression (NMS) on the boxes according to their intersection-over-union(IoU).

    Args:
        boxes: tensor of shape ``(N, 4)``; the boxes to perform nms on; each box is expected to be in ``(x1, y1, x2, y2)`` format.
        iou_thresh: IoU threshold for overlapping.
        scores: tensor of shape ``(N,)``, the score of boxes.
        max_output: the maximum number of boxes to keep; it is optional if this operator is not traced
            otherwise it required to be specified; if it is not specified, all boxes are kept.

    Returns:
        indices of the elements that have been kept by NMS, sorted by scores.

    Note:
        max_output should be specified and should have valid positive value under tracing.

    Examples:
        >>> import numpy as np
        >>> x = np.zeros((100,4))
        >>> np.random.seed(42)
        >>> x[:,:2] = np.random.rand(100,2)*20
        >>> x[:,2:] = np.random.rand(100,2)*20 + 100
        >>> scores = Tensor(np.random.rand(100))
        >>> inp = Tensor(x)
        >>> F.vision.nms(inp, scores, iou_thresh=0.7)
        Tensor([75 69], dtype=int32, device=xpux:0)
    """
    # hack for atlas
    is_atlas = False
    device_type = str(boxes.device)
    if "atlas" in device_type:
        is_atlas = True
        boxes = boxes.to("cpux")
        scores = scores.to("cpux")

    assert (
        boxes.ndim == 2 and boxes.shape[1] == 4
    ), "the expected shape of boxes is (N, 4)"
    assert scores.ndim == 1, "the expected shape of scores is (N,)"
    assert (
        boxes.shape[0] == scores.shape[0]
    ), "number of boxes and scores are not matched"

    boxes = boxes.detach()
    scores = scores.detach()
    sorted_idx = argsort(scores, descending=True)
    boxes = boxes[sorted_idx]

    if max_output is None:
        max_output = boxes.shape[0]

    op = builtin.NMSKeep(iou_thresh, max_output)
    inp = (boxes.reshape(1, -1, 4),)
    indices, count = apply(op, *inp)
    indices = indices[0][: count[0]]
    keep_inds = sorted_idx[indices]
    if is_atlas:
        keep_inds = keep_inds.to(device_type)
    return keep_inds


def remap(
    inp: Tensor,
    map_xy: Tensor,
    border_mode: str = "replicate",
    scalar: float = 0.0,
    interp_mode: str = "linear",
) -> Tensor:
    r"""Applies remap transformation to batched 2D images. Remap is an operation that relocates pixels in a image to another location in a new image.

    The input images are transformed to the output images by the tensor ``map_xy``.
    The output's H and W are same as ``map_xy``'s H and W.

    Args:
        inp: input image, its shape represents ``[b, c, in_h, in_w]``.
        map_xy: transformation matrix, its shape shoule be ``[b, o_h, o_w, 2]``. The shape of output is determined by o_h and o_w.
            For each element in output, its value is determined by inp and ``map_xy``.
            ``map_xy[..., 0]`` and ``map_xy[..., 1]`` are the positions of
            the current element in inp, respectively. Therefore, their ranges are ``[0, in_w - 1]`` and ``[0, in_h - 1]``.
        border_mode: pixel extrapolation method. Default: "replicate". Currently also support "constant", "reflect", "reflect_101", "wrap".
            "replicate": repeatedly fills the edge pixel values of the duplicate image, expanding the new boundary pixel values with
            the edge pixel values.
            "constant": fills the edges of the image with a fixed numeric value.
        scalar: value used in case of a constant border. Default: 0
        interp_mode: interpolation methods. Default: "linear". Currently also support "nearest" mode.

    Returns:
        output tensor. [b, c, o_h, o_w]

    Examples:
        >>> import numpy as np
        >>> inp_shape = (1, 1, 4, 4)
        >>> inp = Tensor(np.arange(16, dtype=np.float32).reshape(inp_shape))
        >>> map_xy_shape = (1, 2, 2, 2)
        >>> map_xy = Tensor(np.array([[[1., 0.],[0., 1.]],
        ...                     [[0., 1.],[0., 1.]]],
        ...                     dtype=np.float32).reshape(map_xy_shape))
        >>> out = F.vision.remap(inp, map_xy)
        >>> out.numpy()
        array([[[[1., 4.],
                 [4., 4.]]]], dtype=float32)
    """
    format = "NCHW"

    op = builtin.Remap(
        imode=interp_mode, border_type=border_mode, format=format, scalar=scalar
    )
    assert isinstance(inp, (Tensor, megbrain_graph.VarNode)), "inp must be Tensor type"
    (result,) = apply(op, inp, map_xy)
    return result


def warp_affine(
    inp: Tensor,
    mat: Tensor,
    out_shape: Union[Tuple[int, int], int, Tensor],
    border_mode: str = "replicate",
    border_val: float = 0.0,
    format: str = "NHWC",
    interp_mode: str = "linear",
) -> Tensor:
    r"""Batched affine transformation on 2D images. Affine transformation is a linear transformation between two-dimensional coordinates.

    Args:
        inp: input image.
        mat: `(batch, 2, 3)` transformation matrix.
        out_shape: output tensor shape.
        border_mode: pixel extrapolation method.
            Default: "replicate". Currently "constant", "reflect",
            "reflect_101", "isolated", "wrap", "replicate", "transparent" are supported.
        border_val: value used in case of a constant border. Default: 0
        format: NHWC" as default based on historical concerns,
            "NCHW" is also supported. Default: "NHWC".
        interp_mode: interpolation methods. Could be "linear", "nearest", "cubic", "area".
            Default: "linear".

    Returns:
        output tensor.

    Note:
        Here all available options for params are listed,
        however it does not mean that you can use all the combinations.
        On different platforms, different combinations are supported.
        ``warp_affine`` only support forward inference, Please refer to ``warp_perspective`` if backward is needed.
    """
    op = builtin.WarpAffine(
        border_mode=border_mode,
        border_val=border_val,
        format=format,
        imode=interp_mode,
    )
    out_shape = utils.astensor1d(out_shape, inp, dtype="int32", device=inp.device)
    (result,) = apply(op, inp, mat, out_shape)
    return result


def warp_perspective(
    inp: Tensor,
    mat: Tensor,
    out_shape: Union[Tuple[int, int], int, Tensor],
    mat_idx: Optional[Union[Iterable[int], Tensor]] = None,
    border_mode: str = "replicate",
    border_val: float = 0.0,
    format: str = "NCHW",
    interp_mode: str = "linear",
) -> Tensor:
    r"""Applies perspective transformation to batched 2D images. A perspective transformation is a projection of a image onto a new view plane.

    The input images are transformed to the output images by the transformation matrix:

    .. math::
            \text{output}(n, c, h, w) = \text{input} \left( n, c,
                \frac{M_{00}w + M_{01}h + M_{02}}{M_{20}w + M_{21}h + M_{22}},
                \frac{M_{10}w + M_{11}h + M_{12}}{M_{20}w + M_{21}h + M_{22}}
                \right)

    Optionally, we can set ``mat_idx`` to assign different transformations to the same image,
    otherwise the input images and transformations should be one-to-one correnspondence.

    Args:
        inp: input image.
        mat: ``(batch, 3, 3)`` transformation matrix.
        out_shape: ``(h, w)`` size of the output image.
        mat_idx: image batch idx assigned to each matrix. Default: None
        border_mode: pixel extrapolation method.
            Default: "replicate". Currently also support "constant", "reflect",
            "reflect_101", "wrap".
        border_val: value used in case of a constant border. Default: 0
        format: NHWC" is also supported. Default: "NCHW".
        interp_mode: interpolation methods.
            Default: "linear". Currently only support "linear" mode.

    Returns:
        output tensor.

    Note:
        The transformation matrix is the inverse of that used by ``cv2.warpPerspective``.

    Examples:
        >>> import numpy as np
        >>> inp_shape = (1, 1, 4, 4)
        >>> x = Tensor(np.arange(16, dtype=np.float32).reshape(inp_shape))
        >>> M_shape = (1, 3, 3)
        >>> # M defines a translation: dst(1, 1, h, w) = rst(1, 1, h+1, w+1)
        >>> M = Tensor(np.array([[1., 0., 1.],
        ...                      [0., 1., 1.],
        ...                      [0., 0., 1.]], dtype=np.float32).reshape(M_shape))
        >>> out = F.vision.warp_perspective(x, M, (2, 2))
        >>> out.numpy()
        array([[[[ 5.,  6.],
                 [ 9., 10.]]]], dtype=float32)
    """
    if inp.dtype == np.float32:
        mat = mat.astype("float32")
    if inp.dtype == np.float16:
        inp = inp.astype("float32")
    op = builtin.WarpPerspective(
        imode=interp_mode, bmode=border_mode, format=format, border_val=border_val
    )
    out_shape = astensor1d(out_shape, inp, dtype="int32", device=inp.device)
    if mat_idx is not None:
        mat_idx = astensor1d(mat_idx, inp, dtype="int32", device=inp.device)
        (result,) = apply(op, inp, mat, mat_idx, out_shape)
        return result
    (result,) = apply(op, inp, mat, out_shape)
    return result


def interpolate(
    inp: Tensor,
    size: Optional[Union[int, Tuple[int, int]]] = None,
    scale_factor: Optional[Union[float, Tuple[float, float]]] = None,
    mode: str = "bilinear",
    align_corners: Optional[bool] = None,
) -> Tensor:
    r"""Down/up samples the input tensor to either the given size or with the given scale_factor. ``size`` can not coexist with ``scale_factor``.

    Args:
        inp: input tensor.
        size: the size of the output tensor. Default: None
        scale_factor: scaling factor of the output tensor. Default: None
        mode: interpolation methods, acceptable values are:
            "bilinear", "linear", "trilinear", "bicubic" and "nearest". Default: "bilinear"
            "trilinear" is valid only when inp is a 5D-tensor
        align_corners: This only has an effect when ``mode``
            is "bilinear" or "linear". Geometrically, we consider the pixels of the input
            and output as squares rather than points. If set to ``True``, the input
            and output tensors are aligned by the center points of their corner
            pixels, preserving the values at the corner pixels. If set to ``False``,
            the input and output tensors are aligned by the corner points of their
            corner pixels, and the interpolation uses edge value padding for
            out-of-boundary values, making this operation *independent* of input size

    Returns:
        output tensor

    Examples:
        >>> import numpy as np
        >>> x = Tensor(np.arange(1, 5, dtype=np.float32).reshape(1, 1, 2, 2))
        >>> out = F.vision.interpolate(x, [4, 4], align_corners=False)
        >>> out.numpy()
        array([[[[1.  , 1.25, 1.75, 2.  ],
                 [1.5 , 1.75, 2.25, 2.5 ],
                 [2.5 , 2.75, 3.25, 3.5 ],
                 [3.  , 3.25, 3.75, 4.  ]]]], dtype=float32)
        >>> out2 = F.vision.interpolate(x, scale_factor=2.)
        >>> np.testing.assert_allclose(out.numpy(), out2.numpy())
    """
    mode = mode.lower()
    if mode not in ["bilinear", "linear", "trilinear", "bicubic", "nearest"]:
        raise ValueError("unsupported interpolate mode: {}".format(mode))
    if mode not in ["bilinear", "linear", "trilinear"]:
        if align_corners is not None:
            raise ValueError(
                "align_corners option can only be set in the bilinear/linear interpolating mode"
            )
    else:
        if align_corners is None:
            align_corners = False

    if mode == "linear":
        inp = expand_dims(inp, 3)

    if mode == "trilinear":
        assert (
            inp.ndim == 5
        ), "under trilinear mode, input tensor must have 5 dimensions"
    else:
        assert (
            inp.ndim == 4
        ), "shape of input tensor must correspond to the operartion mode"

    def get_dsize(scale_factor):
        if isinstance(scale_factor, (float, int)):
            scale_factor = float(scale_factor)
            if mode == "linear":
                scale_factor = (scale_factor, float(1))
            elif mode == "trilinear":
                scale_factor = (scale_factor, scale_factor, scale_factor)
            else:
                scale_factor = (scale_factor, scale_factor)
        else:
            if mode == "linear":
                raise ValueError(
                    "under linear mode, scale_factor can only be single value"
                )

        if mode == "trilinear":
            assert (
                len(scale_factor) == 3
            ), f"shape of scale_factor of interpolate-{mode} must be equal to (3, )"
        else:
            assert (
                len(scale_factor) == 2
            ), f"shape of scale_factor of interpolate-{mode} must be equal to (2, )"
        assert all(
            isinstance(x, (float, int)) for x in scale_factor
        ), f"scale_factor of interpolate must be float/int type"
        dsize = [
            floor(
                Tensor(
                    inp.shape[i + 2] * float(scale_factor[i]),
                    dtype="float32",
                    device=inp.device,
                )
            )
            for i in range(len(scale_factor))
        ]
        dsize = concat(dsize, axis=0)
        return dsize

    if size is None:
        if scale_factor is None:
            raise ValueError("scale_factor must not be None when size is None")
        dsize = get_dsize(scale_factor)

    else:
        if scale_factor is not None:
            raise ValueError("scale_factor must be None when size is provided")

        if isinstance(size, int):
            if mode == "trilinear":
                size = (size, 1, 1)
            else:
                size = (size, 1)
        else:
            if mode == "linear":
                raise ValueError("under linear mode, size can only be single value")
        dsize = size

    if mode == "trilinear":
        if inp.dtype == np.float16:
            inp = inp.astype("float32")
        op = builtin.Resize3D(
            imode="linear", format="NCDHW", align_corners=align_corners
        )
        shape = astensor1d(dsize, inp, dtype="int32", device=inp.device)
        (ret,) = apply(op, inp, shape)
    elif not align_corners:
        # fastpath for interpolate
        mode_map = {
            "linear": "linear",
            "bilinear": "linear",
            "nearest": "nearest",
            "bicubic": "cubic",
        }
        if inp.dtype == np.float16:
            inp = inp.astype("float32")
        # Currently resize only support NCHW mode
        format = "NCHW"
        op = builtin.Resize(imode=mode_map[mode], format=format)
        shape = astensor1d(dsize, inp, dtype="int32", device=inp.device)
        (ret,) = apply(op, inp, shape)
    else:
        assert mode in [
            "linear",
            "bilinear",
        ], "align_corners only support linear or bilinear mode"
        oh, ow = dsize[0], dsize[1]
        ih, iw = inp.shape[2], inp.shape[3]
        hscale = (ih - 1.0) / (oh - 1.0)
        wscale = 1.0 * iw / ow
        if mode != "linear":
            wscale = (iw - 1.0) / (ow - 1.0)
        row0 = concat(
            [
                Tensor(wscale, dtype="float32", device=inp.device),
                Tensor([0, 0], dtype="float32", device=inp.device),
            ],
            axis=0,
        ).reshape(1, 3)
        zeros = Tensor([0], dtype="float32", device=inp.device)
        row1 = concat(
            [zeros, Tensor(hscale, dtype="float32", device=inp.device), zeros], axis=0,
        ).reshape(1, 3)
        weight = concat(
            [row0, row1, Tensor([[0, 0, 1]], dtype="float32", device=inp.device)],
            axis=0,
        ).reshape(1, 3, 3)
        weight = broadcast_to(weight, (inp.shape[0], 3, 3))

        ret = warp_perspective(inp, weight, dsize, interp_mode="linear")

    if mode == "linear":
        ret = reshape(ret, ret.shape[0:3])
    return ret


def nvof(src: Tensor, precision: int = 1) -> Tensor:
    r"""Implements NVIDIA Optical Flow SDK.

    Args:
        src: input tensor with shape (n, t, h, w, c4) and unit8 dtype.
        precision: 0:NV_OF_PERF_LEVEL_SLOW 1:NV_OF_PERF_LEVEL_MEDIUM 2:NV_OF_PERF_LEVEL_FAST.

    Returns:
        output tensor with shape: ``(n, t-1, (h+out_grid_size-1)//out_grid_size, (w+out_grid_size-1)//out_grid_size, c2)``.
        By default, out_grid_size = 4. dtype: int16.
    """
    assert src.ndim == 5 and src.shape[4] == 4

    src = src.detach()

    op = builtin.NvOf(precision=precision)
    return apply(op, src)[0]


def flip(inp: Tensor, vertical: bool = True, horizontal: bool = True,) -> Tensor:
    r"""Reverse an n-dimensional tensor according to the given parameters

    Args:
        inp(Tensor): input images, format must be nhwc.
        vertical(bool, optional): Flip vertically or not. Default: True.
        horizontal(bool, optional): Flip horizontally or not. Default: True.

    Returns:
        Reversed result.

    Examples:
        >>> import numpy as np
        >>> x = Tensor(np.arange(0, 4, dtype=np.float32).reshape(1, 2, 2, 1))
        >>> y = F.vision.flip(x)
        >>> y.numpy()
        array([[[[3.],
                [2.]],
                [[1.],
                 [0.]]]], dtype=float32)
    """
    op = builtin.Flip(horizontal=horizontal, vertical=vertical)
    (result,) = apply(op, inp)
    return result


def rot90(inp: Tensor, clockwise: bool = True,) -> Tensor:
    r"""Rotate an n-D tensor by 90 degrees.

    Args:
        inp(Tensor): input image, format must be nwhc.
        clockwise(bool, optional): Rotate 90° clockwise or 90° counterclockwise. Default: True.

    Returns:
        rotated result.

    Examples:
        >>> import numpy as np
        >>> x = Tensor(np.arange(0, 4, dtype=np.float32).reshape(1, 2, 2, 1))
        >>> y = F.vision.rot90(x)
        >>> y.numpy()
        array([[[[2.],
                [0.]],
                [[3.],
                 [1.]]]], dtype=float32)
    """
    op = builtin.Rotate(clockwise=clockwise)
    (result,) = apply(op, inp)
    return result


def rotate(
    inp: Tensor,
    angle: float = 0.0,
    format: str = "NCHW",
    interp_mode: str = "bilinear",
) -> Tensor:
    r"""Rotate a tensor by given angle.

    Args:
        inp(Tensor): input image, format must be NCHW or NHWC.
        angle(float): rotation angle of the image.
        format(str, optional): format of the input tensor, currently only supports NCHW and NHWC.
        interp_mode(str, optional): interpoloation mode, currently only supports bilinear for NCHW format
            and area mode for NHWC format.

    Returns:
        rotated result.
    """
    if format == "NCHW":
        assert (
            interp_mode == "bilinear"
        ), "Currently NCHW format only supports bilinear mode."
        interp_mode = interp_mode[2:]
        height, width = inp.shape[2:]
    elif format == "NHWC":
        assert interp_mode == "area", "Currently NHWC format only supports area mode."
        height, width = inp.shape[1:3]

    angle = angle / 180.0 * np.pi

    rotate_matrix = Tensor(
        [
            [
                [
                    cos(angle),
                    -sin(angle),
                    (1 - cos(angle)) * ((width - 1) / 2)
                    + sin(angle) * ((height - 1) / 2),
                ],
                [
                    sin(angle),
                    cos(angle),
                    -sin(angle) * ((width - 1) / 2)
                    + (1 - cos(angle)) * ((height - 1) / 2),
                ],
            ]
        ]
    )

    out_shape = (height, width)
    output = warp_affine(
        inp,
        rotate_matrix,
        out_shape,
        border_mode="constant",
        format=format,
        interp_mode=interp_mode,
    )

    return output


def resize(
    inp: Tensor,
    size: Iterable[int],
    format: str = "NCHW",
    imode: str = "bilinear",
    max_size: Optional[int] = None,
) -> Tensor:
    r"""Resize the input image to the given size.

    Args:
        inp(Tensor): input images.
        size(Tensor): Desired output size.
        format(str, optional): default="NCHW", "NHWC" is also supported.
        imode(str, optional): interpolation mode, default="bilinear", "nearest" and "bicubic" are also supported.
        max_size(int, optional): The maximum allowed for the longer edge of the resized image. If the longer edge
            of the image is greater than ``max_size`` after being resized according to ``size``, ``size`` will be 
            overruled so that the longer edge is equal to ``max_size``.
    Returns:
        resized result.

    Examples:
        >>> import numpy as np
        >>> x = Tensor(np.arange(0, 16, dtype=np.float32).reshape(1, 1, 4, 4))
        >>> size = Tensor([2, 2])
        >>> y = F.vision.resize(x, size)
        >>> y.numpy()
        array([[[[ 2.5,  4.5],
         [10.5, 12.5]]]], dtype=float32)
    """
    if imode in ["bilinear", "bicubic"]:
        imode = imode[2:]
    op = builtin.Resize(format=format, imode=imode)
    if max_size is not None:
        size = [max_size if x > max_size else x for x in size]
    size = Tensor(size)
    (result,) = apply(op, inp, size)
    return result
