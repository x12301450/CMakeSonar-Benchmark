# -*- coding: utf-8 -*-
import collections
import gc
import itertools
import multiprocessing
import os
import pickle
import platform
import queue
import random
import sys
import threading
import time
import traceback
from multiprocessing import Array, Value

import numpy as np

from ..device import _sh, get_default_device
from ..functional.tensor import copy
from ..logger import call_countlog_func, get_logger
from ..random.rng import _random_seed_generator
from ..tensor import Tensor
from .collator import Collator
from .dataset import Dataset, StreamDataset
from .sampler import MapSampler, Sampler, SequentialSampler, StreamSampler
from .signal_handling import (
    _reset_worker_pids,
    _set_sigchld_handler,
    _set_worker_pids,
    _set_worker_signal_handlers,
)
from .transform import CudaTransform, PseudoTransform, Transform

try:
    import thread
except:
    import _thread as thread

logger = get_logger(__name__)


GLOBAL_TIMEOUT = 5

# Use to enable DataLoader monitoring
data_monitor = int(os.environ.get("MGE_DATA_MONITOR", "0"))

# This variable controls the interval of iterations for printing the monitored time. Default: 10.
# Currently only supported when DataLoader num_workers is greater than 0.
data_monitor_frequency = int(os.environ.get("MGE_DATA_MONITOR_FREQUENCY", "10"))

if data_monitor:
    # The number of processes to be monitored, greater than or equal to num_workers of dataloader
    monitor_num_workers = Value("i", 16)

    # Each monitor_workers represents the process id of a dataloader process
    monitor_workers = Array("i", [0] * monitor_num_workers.value)

    # When Parallel, the main process needs to obtain the time to communicate with the child process,
    # and IPC_worker is the process id of the main process
    IPC_worker = Value("i", 0)

    # When Parallel, get the time when each child process put data
    put_time = Array("d", [0.0] * monitor_num_workers.value)

    # The time to get the dataset for each step
    dataset_time = 0.0
    # The time to transform data for each step
    transform_time = 0.0
    # The time to collate data for each step
    collate_time = 0.0


def get_monitor_time_stats(func, *args, **kwargs):
    if data_monitor:
        tik = time.perf_counter()
        ret = func(*args, **kwargs)
        tok = time.perf_counter()
        return ret, tok - tik
    else:
        ret = func(*args, **kwargs)
        return ret, 0


def raise_timeout_error():
    raise RuntimeError("dataloader timeout")


class DataLoader:
    r"""Data loader. Combines a dataset and a sampler, and provides a convenient way
    to iterate on a given dataset. The process is as follows:

    .. mermaid::
       :align: center
    
       flowchart LR
          Dataset.__len__ -- Sampler --> Indices
          batch_size -- Sampler --> Indices
          Indices -- Dataset.__getitem__ --> Samples
          Samples -- Transform + Collator --> mini-batch

    See :ref:`data-guide` for more details.

    Args:
        dataset: dataset from which to load the minibatch.
        sampler: defines the strategy to sample data from the dataset.
            If ``None``, it will sequentially sample from the dataset one by one.
        transform: defined the transforming strategy for a sampled batch.
        collator: defined the merging strategy for a transformed batch.
        num_workers: the number of sub-process to load, transform and collate
            the batch. ``0`` means using single-process. Default: 0
        timeout: if positive, means the timeout value(second) for collecting a
            batch from workers. Default: 0
        preload: whether to enable the preloading strategy of the dataloader. 
            When enabling, the dataloader will preload one batch to the device memory to speed up the whole training process.
        parallel_stream: whether to splitting workload across all workers when dataset is streamdataset and num_workers > 0.
            When enabling, each worker will collect data from different dataset in order to speed up the whole loading process.
            See ref:`streamdataset-example` for more details

    Examples:
        >>> import megengine.data as data
        >>> dataset = CustomDataset()         # doctest: +SKIP
        >>> dataloader = DataLoader(dataset)  # doctest: +SKIP
        >>> for batch_data in DataLoader:     # doctest: +SKIP
        >>>     print(batch_data.shape)       # doctest: +SKIP

    .. admonition:: The effect of enabling preload
       :class: warning

       * All elements in :class:`map`, :class:`list`, and :class:`tuple` will be converted to :class:`~.Tensor` by preloading,
         and you will get :class:`~.Tensor` instead of the original Numpy array or Python built-in data structrure.
       * Tensors' host2device copy and device kernel execution will be overlapped,
         which will improve the training speed at the cost of **higher device memory usage** (due to one more batch data on device memory).
         This feature saves more time when your NN training time is short or your machine's host PCIe bandwidth for each device is low.
    """

    def __init__(
        self,
        dataset: Dataset,
        sampler: Sampler = None,
        transform: Transform = None,
        collator: Collator = None,
        num_workers: int = 0,
        timeout: int = 0,
        preload: bool = False,
        parallel_stream: bool = False,
    ):
        if num_workers < 0:
            raise ValueError("num_workers should not be negative")

        if timeout < 0:
            raise ValueError("timeout should not be negative")

        self.dataset = dataset
        self.num_workers = num_workers
        self.timeout = timeout
        self.preload = preload
        self.parallel_stream = parallel_stream

        if isinstance(dataset, StreamDataset):
            self.sampler = sampler if sampler else StreamSampler(batch_size=1)
            assert isinstance(
                self.sampler, StreamSampler
            ), "types of dataset and sampler do not match"
            if parallel_stream is False and self.num_workers > 1:
                logger.warning(
                    "Data time will be affected by getting origin-data, please set parallel_stream in order to speed up dataloader!"
                )
            self.datakind = "stream"
        else:
            assert isinstance(
                dataset, Dataset
            ), "Can not recognize this kind of dataset: %s" % type(dataset)
            self.sampler = (
                sampler
                if sampler
                else SequentialSampler(dataset, batch_size=1, drop_last=False)
            )
            assert isinstance(
                self.sampler, MapSampler
            ), "types of dataset and sampler do not match"
            self.datakind = "map"

        if transform is None:
            self.transform = PseudoTransform()
        else:
            self.transform = transform

        if collator is None:
            self.collator = Collator()
        else:
            self.collator = collator

        if platform.system() == "Linux" and self.num_workers > 0:
            self.check_memory_rationality()

    def __iter__(self):
        if platform.system() == "Windows" and self.num_workers > 0:
            print(
                "pyarrow.plasma does not support ParallelDataLoader on windows, changing num_workers to be zero"
            )
            self.num_workers = 0
        if os.getenv("TERMUX_VERSION"):
            # FIXME: termux install pyarrow will build error now
            # remove this logic after pyarrow fix this issue
            print(
                "pyarrow do not support on termux env now, changing num_workers to be zero"
            )
            self.num_workers = 0
        if isinstance(self.dataset, StreamDataset):
            if not self.num_workers:
                return _SerialStreamDataLoaderIter(self, self.preload)
            else:
                return _ParallelStreamDataLoaderIter(self, self.preload)
        else:
            assert isinstance(
                self.dataset, Dataset
            ), "Can not recognize this kind of dataset: %s" % type(self.dataset)
            if not self.num_workers:
                return _SerialMapDataLoaderIter(self, self.preload)
            else:
                return _ParallelMapDataLoaderIter(self, self.preload)

    def __len__(self):
        return len(self.sampler)

    def check_memory_rationality(self):
        import psutil

        main_memory = psutil.Process(os.getpid()).memory_info().rss / 1024 / 1024 / 1024
        total_memory = (self.num_workers + 1) * main_memory
        limit_in_bytes = os.popen("cat /sys/fs/cgroup/memory/memory.limit_in_bytes")
        current_memory = int(limit_in_bytes.read()) / 1024 / 1024 / 1024
        limit_in_bytes.close()
        if current_memory < total_memory:
            logger.warning(
                "Each worker need to read the shared meta-data, which will be increasing the reference count."
                "Copy-On-Write propety will lead to 'memory leak', the memory usage will end up being "
                + str(total_memory)
                + " GB, "
                "However the current requested memory is "
                + str(current_memory)
                + " GB, "
                "Maybe you can request more memory or uesd np-array to save meta-data rather than List or Tuple"
            )


class PreLoader:
    def __init__(self, loader, preload):
        self.dataset = loader.dataset
        self.sampler = loader.sampler
        self.seed = _random_seed_generator().__next__()
        self.transform = loader.transform
        self.collator = loader.collator
        self.num_workers = loader.num_workers
        self.timeout = loader.timeout
        self.num_processed = 0
        self.datakind = loader.datakind
        self.parallel_stream = loader.parallel_stream

        if preload:
            self.default_device = get_default_device()
            self.pre_load_device = self.default_device + ":" + str(_sh.get_next())
            self.pre_load_device_cache = None
        self.preload = preload

        if data_monitor:
            global monitor_num_workers, monitor_workers, put_time
            monitor_num_workers.value = self.num_workers
            monitor_workers = Array("i", [0] * monitor_num_workers.value)
            put_time = Array("d", [0.0] * monitor_num_workers.value)

    def __iter__(self):
        return self

    """
    strategy one: load from numpy data, and generate dtype tensor
    """

    def _load_tensor(self, batch, cached=True):
        if isinstance(batch, np.ndarray):
            device = self.pre_load_device if cached else self.default_device
            return Tensor(batch, device=device)
        elif isinstance(batch, collections.abc.Mapping):
            return {k: self._load_tensor(v, cached) for k, v in batch.items()}
        elif isinstance(batch, tuple) and hasattr(batch, "_fields"):  # namedtuple
            return type(batch)(*(self._load_tensor(value, cached) for value in batch))
        elif isinstance(batch, collections.abc.Sequence):
            return [self._load_tensor(value, cached) for value in batch]
        else:
            return batch

    """
    strategy two: load from cache that is already tensor just do d2d copy
    """

    def _load_cache(self, data):
        if isinstance(data, Tensor):
            if data.device == self.default_device:
                return data
            return copy(data, device=self.default_device)
        elif isinstance(data, collections.abc.Mapping):
            return {k: self._load_cache(v) for k, v in data.items()}
        elif isinstance(data, tuple) and hasattr(data, "_fields"):  # namedtuple
            return type(data)(*(self._load_cache(value) for value in data))
        elif isinstance(data, collections.abc.Sequence):
            return [self._load_cache(value) for value in data]
        else:
            return data

    def _swap_out_cache(self):
        out = self._load_cache(self.pre_load_device_cache)
        self.pre_load_device_cache = None  # clean cache
        return out


class _ParallelDataLoaderIter:
    def __init__(self):
        self._worker_queue_idx_cycle = itertools.cycle(range(self.num_workers))
        if isinstance(self.transform, CudaTransform):
            self.transform.apply = call_countlog_func(
                self.transform.apply,
                1,
                "Using CudaTransform now, not forget to convert input to mge.tensor",
            )
        from .tools._queue import PlasmaShmQueue

        self._worker_result_queue = PlasmaShmQueue()
        self._shutdown = False
        self._workers_done_event = multiprocessing.Event()
        self._index_queues = []
        self._workers = []

        for i in range(self.num_workers):
            index_queue = multiprocessing.Queue()
            index_queue.cancel_join_thread()
            w = multiprocessing.Process(
                target=_worker_loop,
                args=(
                    self.dataset,
                    index_queue,
                    self._worker_result_queue,
                    self._workers_done_event,
                    self.transform,
                    self.collator,
                    self.sampler.batch_size,
                    self.seed + i,
                    i,
                    self.num_workers,
                    self.datakind,
                    self.parallel_stream,
                ),
                daemon=True,
            )
            gc.collect()
            w.start()
            self._index_queues.append(index_queue)
            self._workers.append(w)

        self._data_queue = self._worker_result_queue
        self._worker_pids_set = False
        _set_worker_pids(id(self), tuple(w.pid for w in self._workers))
        _set_sigchld_handler()
        self._worker_pids_set = True
        self._reset()

    def _try_put_index(self):
        raise NotImplementedError

    def _reset(self):
        self._sampler_iter = iter(self.sampler)

        self._send_idx = 0
        self._rcvd_idx = 0

        self._task_info = {}
        self._workers_status = [True for _ in range(self.num_workers)]
        for _ in range(2 * self.num_workers):
            self._try_put_index()

    def _process_data(self, data):
        self._rcvd_idx += 1
        self._try_put_index()
        if isinstance(data, bytes):
            data = pickle.loads(data)
            data.reraise()

        if isinstance(self.transform, CudaTransform):
            data = self.transform.apply(data)

        return data

    def _get_data(self):
        if self.timeout > 0:
            success, data = self._try_get_data(self.timeout)
            if success:
                return data
            else:
                raise_timeout_error()
        else:
            while True:
                success, data = self._try_get_data()
                if success:
                    return data

    def _get_next_batch(self):
        while True:
            while self._rcvd_idx < self._send_idx:
                info = self._task_info[self._rcvd_idx]
                worker_id = info[0]
                if (
                    len(info) == 2 or self._workers_status[worker_id]
                ):  # has data or work is still active
                    break
                del self._task_info[self._rcvd_idx]
                self._rcvd_idx += 1
            else:
                self._shutdown_workers()
                raise StopIteration

            if len(self._task_info[self._rcvd_idx]) == 2:
                data = self._task_info.pop(self._rcvd_idx)[1]
                return self._process_data(data)

            _get_data = self._get_data()
            if len(_get_data) == 1:
                assert isinstance(_get_data[0], bytes)
                exception = pickle.loads(_get_data[0])
                exception.reraise()
                self._try_put_index()
                continue
            else:
                idx, data = _get_data

            if isinstance(data, int):  # Check if StopIteration in StreamDataset
                self._mark_worker_as_unavailable(data)
                self._try_put_index()
                continue

            if idx != self._rcvd_idx:
                self._task_info[idx] += (data,)
            else:
                del self._task_info[idx]
                return self._process_data(data)

    def _try_get_data(self, timeout=GLOBAL_TIMEOUT):
        try:
            data = self._data_queue.get(timeout=timeout)

            if data_monitor:
                pid = os.getpid()
                idx = data[0]

                if IPC_worker.value == 0:
                    IPC_worker.value = pid

                if pid == IPC_worker.value and idx % data_monitor_frequency == 0:
                    data_tok = time.perf_counter()
                    worker_idx = idx % monitor_num_workers.value
                    IPC_time = data_tok - put_time[worker_idx]
                    print(f"pid: {pid}, idx: {idx}, IPC_time: {IPC_time:.6f}")

            return (True, data)
        except Exception as e:
            failed_workers = []
            for worker_id, w in enumerate(self._workers):
                if self._workers_status[worker_id] and not w.is_alive():
                    failed_workers.append((worker_id, w))
                    self._mark_worker_as_unavailable(worker_id)
                    if w.exitcode == -9:
                        logger.debug(
                            "Maybe memory is not enough, please request for more memory!"
                        )
            if len(failed_workers) > 0:
                pids_str = ", ".join(str(w_info[1].pid) for w_info in failed_workers)
                w_ids_str = ", ".join(str(w_info[0]) for w_info in failed_workers)
                exitcode_str = ", ".join(
                    str(w_info[1].exitcode) for w_info in failed_workers
                )
                raise RuntimeError(
                    "DataLoader worker (worker(s): {} , pid(s): {}) exited unexpectedly, exitcode(s): {}".format(
                        w_ids_str, pids_str, exitcode_str
                    )
                )

            if isinstance(e, queue.Empty):
                return (False, None)

    def _mark_worker_as_unavailable(self, worker_id, shutdown=False):
        q = self._index_queues[worker_id]
        q.put(None)
        self._workers_status[worker_id] = False
        assert self._workers_done_event.is_set() == shutdown

    def _shutdown_workers(self):
        if not self._shutdown:
            self._shutdown = True
            try:
                self._workers_done_event.set()
                for worker_id in range(len(self._workers)):
                    if self._workers_status[worker_id]:
                        self._mark_worker_as_unavailable(worker_id, shutdown=True)
                for w in self._workers:
                    if w.is_alive():
                        w.terminate()
                    w.join(timeout=GLOBAL_TIMEOUT)
                for q in self._index_queues:
                    q.cancel_join_thread()
                    q.close()
                self._data_queue.cancel_join_thread()
                self._data_queue.close()
            finally:
                if self._worker_pids_set:
                    _reset_worker_pids(id(self))
                    self._worker_pids_set = False

    def __del__(self):
        self._shutdown_workers()


class _BaseMapDataLoaderIter(PreLoader):
    def __init__(self, loader, preload):
        super().__init__(loader, preload)

    def __len__(self):
        return len(self.sampler)

    def __next__(self):
        if self.preload:
            cached = self.pre_load_device_cache
            if cached is None:  # first and last
                if self.num_processed >= len(self):  # last
                    raise StopIteration
                elif self.num_processed == 0:  # first
                    self._try_load_tensor(cached=False)  # first do the h2d
            out = self._swap_out_cache()
            self._try_load_tensor()
            return out
        else:
            data = self._get_next_batch()
            return data

    def _try_load_tensor(self, cached=True):
        if self.num_processed >= len(self):
            return
        else:
            self.num_processed += 1
            batch = self._get_next_batch()
            self.pre_load_device_cache = self._load_tensor(batch, cached)


class _SerialMapDataLoaderIter(_BaseMapDataLoaderIter):
    def __init__(self, loader, preload):
        super(_SerialMapDataLoaderIter, self).__init__(loader, preload)
        self._sampler_iter = iter(self.sampler)

    def _get_next_batch(self):
        if data_monitor:
            global monitor_workers
            pid = os.getpid()
            if monitor_workers[0] == 0:
                monitor_workers[0] = pid

            data_tik = time.perf_counter()
            indices = next(self._sampler_iter)
            items = [self.dataset[idx] for idx in indices]
            data_tok = time.perf_counter()
            dataset_time = data_tok - data_tik

            trans_items, transform_time = get_monitor_time_stats(
                self.transform.apply_batch, items
            )
            collate_ret, collate_time = get_monitor_time_stats(
                self.collator.apply, trans_items
            )
            print(
                f"pid: {pid}, dataset_time: {dataset_time:.6f}, transform_time: {transform_time:.6f}, collate_time: {collate_time:.6f}"
            )
            return collate_ret
        else:
            indices = next(self._sampler_iter)
            items = [self.dataset[idx] for idx in indices]
            trans_items = self.transform.apply_batch(items)
            return self.collator.apply(trans_items)


class _ParallelMapDataLoaderIter(_BaseMapDataLoaderIter, _ParallelDataLoaderIter):
    def __init__(self, loader, preload):
        _BaseMapDataLoaderIter.__init__(self, loader, preload)
        _ParallelDataLoaderIter.__init__(self)

    def _try_put_index(self):
        try:
            index = next(self._sampler_iter)
        except StopIteration:
            return
        for _ in range(self.num_workers):  # find the next active worker, if any
            worker_queue_idx = next(self._worker_queue_idx_cycle)
            if self._workers_status[worker_queue_idx]:
                break

        self._index_queues[worker_queue_idx].put((self._send_idx, index))
        self._task_info[self._send_idx] = (worker_queue_idx,)
        self._send_idx += 1


_worker_info = None


class WorkerInfo(object):
    __initialized = False

    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)
        self.__keys = tuple(kwargs.keys())
        self.__initialized = True

    def __setattr__(self, key, val):
        if self.__initialized:
            raise RuntimeError(
                "Cannot assign attributes to {} objects".format(self.__class__.__name__)
            )
        return super(WorkerInfo, self).__setattr__(key, val)

    def __repr__(self):
        items = []
        for k in self.__keys:
            items.append("{}={}".format(k, getattr(self, k)))
        return "{}({})".format(self.__class__.__name__, ", ".join(items))


def get_worker_info():
    return _worker_info


class _BaseStreamDataLoaderIter(PreLoader):
    def __init__(self, loader, preload):
        super().__init__(loader, preload)
        self.dataset_iter = iter(self.dataset)

    def __next__(self):
        if self.preload:
            if self.pre_load_device_cache is None:
                self._try_load_tensor(cached=False)  # load in current
            out = self._swap_out_cache()
            self._try_load_tensor()  # load in cached
            return out
        else:
            return self._get_next_batch()

    def _try_load_tensor(self, cached=True):
        batch = self._get_next_batch()
        self.pre_load_device_cache = self._load_tensor(batch, cached)


class _SerialStreamDataLoaderIter(_BaseStreamDataLoaderIter):
    def __init__(self, loader, preload):
        super().__init__(loader, preload)
        self.dataset_iter = iter(self.dataset)

    def _try_get_raw_data(self, start_time):
        raw_data = None
        while not raw_data:
            try:
                if self.timeout > 0:
                    timer = threading.Timer(self.timeout, thread.interrupt_main)
                    timer.start()
                raw_data = next(self.dataset_iter)
                if self.timeout > 0:
                    timer.cancel()
            except AttributeError as error:
                raise error
            except:
                if self.timeout > 0:
                    timer.cancel()
                    waited_time = time.time() - start_time
                    if waited_time > self.timeout:
                        raise_timeout_error()
        return raw_data

    def _get_next_batch(self):
        ret = []
        start_time = time.time()

        if data_monitor:
            global monitor_workers
            pid = os.getpid()
            if monitor_workers[0] == 0:
                monitor_workers[0] = pid
            total_dataset_time, total_transform_time = 0, 0
            while len(ret) < self.sampler.batch_size:
                if pid == monitor_workers[0]:
                    raw_data, _dataset_time = get_monitor_time_stats(
                        self._try_get_raw_data, start_time
                    )
                    raw_data, _transform_time = get_monitor_time_stats(
                        self.transform.apply, raw_data
                    )
                    total_dataset_time += _dataset_time
                    total_transform_time += _transform_time
                    ret.append(raw_data)
                else:
                    raw_data = self._try_get_raw_data(start_time)
                    ret.append(self.transform.apply(raw_data))
            if pid == monitor_workers[0]:
                collate_ret, collate_time = get_monitor_time_stats(
                    self.collator.apply, ret
                )
                print(
                    f"pid: {pid}, dataset_time: {total_dataset_time:.6f}, transform_time: {total_transform_time:.6f}, collate_time: {collate_time:.6f}"
                )
            else:
                collate_ret = self.collator.apply(ret)
            return collate_ret
        else:
            while len(ret) < self.sampler.batch_size:
                raw_data = self._try_get_raw_data(start_time)
                ret.append(self.transform.apply(raw_data))
            return self.collator.apply(ret)


class _ParallelStreamDataLoaderIter(_BaseStreamDataLoaderIter, _ParallelDataLoaderIter):
    def __init__(self, loader, preload):
        _BaseStreamDataLoaderIter.__init__(self, loader, preload)
        _ParallelDataLoaderIter.__init__(self)

    def _get_remaind_data(self, place_holder):
        num = self.sampler.batch_size
        for _ in range(num - 1):
            place_holder.append(next(self.dataset_iter))
        return place_holder

    def _try_put_index(self):
        try:
            if self.parallel_stream is False:
                start_time = time.time()
                place_holder = [next(self.dataset_iter)]
                waited_time = time.time() - start_time
                if self.timeout > 0 and waited_time > self.timeout:
                    raise_timeout_error()
                place_holder = self._get_remaind_data(place_holder)
            else:
                place_holder = next(self._sampler_iter)
        except StopIteration:
            return

        for _ in range(self.num_workers):
            worker_queue_idx = next(self._worker_queue_idx_cycle)
            if self._workers_status[worker_queue_idx]:
                break
        else:
            return

        self._index_queues[worker_queue_idx].put((self._send_idx, place_holder))
        self._task_info[self._send_idx] = (worker_queue_idx,)
        self._send_idx += 1


class ManagerWatchdog(object):
    def __init__(self):
        self.manager_pid = os.getppid()
        self.manager_dead = False

    def is_alive(self):
        if not self.manager_dead:
            self.manager_dead = os.getppid() != self.manager_pid
        return not self.manager_dead


def stream_fetcher(
    dataset_iter, place_holder, transform, collate, parallel_stream, batch_size
):
    pid = os.getpid()
    global dataset_time, transform_time, collate_time
    data = []
    total_transform_time, total_dataset_time = 0, 0
    for idx in place_holder:
        try:
            if parallel_stream is False:
                raw_data = idx
            else:
                raw_data, _dataset_time = get_monitor_time_stats(next, dataset_iter)
                total_dataset_time += _dataset_time

            if isinstance(transform, CudaTransform):
                data.append(raw_data)
            else:
                trans_items, _transform_time = get_monitor_time_stats(
                    transform.apply, raw_data
                )
                total_transform_time += _transform_time
                data.append(trans_items)

        except StopIteration:
            break

    if len(data) == 0:
        raise StopIteration

    data, collate_time = get_monitor_time_stats(collate.apply, data)
    if data_monitor:
        dataset_time = total_dataset_time
        transform_time = (
            0.0 if isinstance(transform, CudaTransform) else total_transform_time
        )
        return data, pid, dataset_time, transform_time, collate_time
    else:
        return data


def map_fetcher(dataset, place_holder, transform, collate, parallel_stream, batch_size):
    if data_monitor:
        pid = os.getpid()
        global dataset_time, transform_time, collate_time

        data_tik = time.perf_counter()
        items = [dataset[idx] for idx in place_holder]
        data_tok = time.perf_counter()
        dataset_time = data_tok - data_tik

        trans_items, transform_time = (
            items,
            0.0
            if isinstance(transform, CudaTransform)
            else get_monitor_time_stats(transform.apply_batch, items),
        )
        data, collate_time = get_monitor_time_stats(collate.apply, trans_items)
        return data, pid, dataset_time, transform_time, collate_time
    else:
        items = [dataset[idx] for idx in place_holder]
        if not isinstance(transform, CudaTransform):
            items = transform.apply_batch(items)
        data = collate.apply(items)
        return data


def _worker_loop(
    dataset,
    index_queue,
    data_queue,
    done_event,
    transform,
    collate,
    batch_size,
    seed,
    worker_id,
    num_workers,
    datakind,
    parallel_stream,
):
    _set_worker_signal_handlers()
    random.seed(seed)
    np.random.seed(seed)
    watchdog = ManagerWatchdog()
    iteration_end = False
    fetcher = map_fetcher
    if datakind == "stream":
        global _worker_info
        _worker_info = WorkerInfo(idx=worker_id, worker=num_workers, seed=seed)
        dataset = iter(dataset)
        fetcher = stream_fetcher

    while watchdog.is_alive():
        try:
            r = index_queue.get(timeout=GLOBAL_TIMEOUT)
        except queue.Empty:
            continue
        if r is None:
            assert done_event.is_set() or iteration_end
            break
        elif done_event.is_set() or iteration_end:
            continue

        idx, place_holder = r
        try:
            if data_monitor:
                data, pid, dataset_time, transform_time, collate_time = fetcher(
                    dataset,
                    place_holder,
                    transform,
                    collate,
                    parallel_stream,
                    batch_size,
                )

                if idx < monitor_num_workers.value:
                    if monitor_workers[idx] == 0:
                        monitor_workers[idx] = pid
                worker_idx = idx % monitor_num_workers.value
                if (
                    pid == monitor_workers[worker_idx]
                    and idx % data_monitor_frequency == 0
                ):
                    print(
                        f"pid: {pid}, idx: {idx}, dataset_time: {dataset_time:.6f}, transform_time: {transform_time:.6f}, collate_time: {collate_time:.6f}"
                    )
            else:
                data = fetcher(
                    dataset,
                    place_holder,
                    transform,
                    collate,
                    parallel_stream,
                    batch_size,
                )
        except Exception as e:
            if isinstance(e, StopIteration) and datakind == "stream":
                data = worker_id
                iteration_end = True
            else:
                from .tools._queue import _ExceptionWrapper

                exc_info = sys.exc_info()
                where = "in DataLoader worker process {}".format(worker_id)
                exc_msg = "".join(traceback.format_exception(*exc_info))
                data = _ExceptionWrapper(exc_info[0].__name__, exc_msg, where)
                data = pickle.dumps(data)

        if data_monitor:
            pid = os.getpid()
            worker_idx = idx % monitor_num_workers.value
            if pid == monitor_workers[worker_idx]:
                data_tik = time.perf_counter()
                put_time[worker_idx] = data_tik

        data_queue.put((idx, data))
        del data, idx, place_holder, r

    if done_event.is_set():
        data_queue.disconnect_client()
        data_queue.close()
