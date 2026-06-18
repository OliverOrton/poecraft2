from __future__ import annotations

import ctypes as ct
import gc
import os
import unittest

from test_bindings import ARTIFACT, BASE
from poecraft_engine import load_data


def _private_bytes() -> int | None:
    if os.name != "nt":
        return None

    class ProcessMemoryCountersEx(ct.Structure):
        _fields_ = [
            ("cb", ct.c_uint32),
            ("PageFaultCount", ct.c_uint32),
            ("PeakWorkingSetSize", ct.c_size_t),
            ("WorkingSetSize", ct.c_size_t),
            ("QuotaPeakPagedPoolUsage", ct.c_size_t),
            ("QuotaPagedPoolUsage", ct.c_size_t),
            ("QuotaPeakNonPagedPoolUsage", ct.c_size_t),
            ("QuotaNonPagedPoolUsage", ct.c_size_t),
            ("PagefileUsage", ct.c_size_t),
            ("PeakPagefileUsage", ct.c_size_t),
            ("PrivateUsage", ct.c_size_t),
        ]

    counters = ProcessMemoryCountersEx()
    counters.cb = ct.sizeof(counters)
    kernel32 = ct.WinDLL("kernel32", use_last_error=True)
    psapi = ct.WinDLL("psapi", use_last_error=True)
    kernel32.GetCurrentProcess.restype = ct.c_void_p
    psapi.GetProcessMemoryInfo.argtypes = [
        ct.c_void_p,
        ct.POINTER(ProcessMemoryCountersEx),
        ct.c_uint32,
    ]
    if not psapi.GetProcessMemoryInfo(
        kernel32.GetCurrentProcess(), ct.byref(counters), counters.cb
    ):
        raise ct.WinError(ct.get_last_error())
    return counters.PrivateUsage


@unittest.skipUnless(ARTIFACT.exists(), "compiled engine artifact is absent")
class BatchStressTests(unittest.TestCase):
    def test_repeated_batch_and_handle_lifetimes_are_bounded(self):
        data = load_data(ARTIFACT)
        session = data.create_session(BASE, 86)

        # Warm Python/native allocators and the common weighted-pool cache.
        with session.create_action_context(seed=1) as context:
            warm = context.run_batch(session.create_item(), "alchemy", 1_000)
            context.apply_batch(warm.items, "chaos")
        del warm
        gc.collect()
        baseline = _private_bytes()

        for cycle in range(20):
            with session.create_action_context(seed=cycle + 2) as context:
                batch = context.run_batch(
                    session.create_item(), {"type": "alchemy"}, 1_000
                )
                context.apply_batch(batch.items, {"type": "chaos"})
                context.apply_batch(batch.items, {"type": "exalt"})
            del batch
            if cycle % 5 == 4:
                gc.collect()

        gc.collect()
        final = _private_bytes()
        session.close()
        data.close()

        if baseline is not None and final is not None:
            # Windows heaps retain some arenas. This catches unbounded handle,
            # item-array, or result-array growth while allowing allocator noise.
            self.assertLess(final - baseline, 32 * 1024 * 1024)
