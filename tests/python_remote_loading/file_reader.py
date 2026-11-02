import threading
import time


def _strided_view(chunk):
    interleaved = bytearray(len(chunk) * 2)
    interleaved[::2] = chunk
    return memoryview(interleaved)[::2]


class FileReader:
    """A byte-range reader over a local file that logs every call, standing in for S3."""

    def __init__(self, path, delay=0.0, fail_on_call=None, short=False, missing_chunk=False,
                 strided=False):
        with open(path, 'rb') as f:
            self._data = f.read()
        self._delay = delay
        self._fail_on_call = fail_on_call
        self._short = short
        self._missing_chunk = missing_chunk
        self._strided = strided
        self._lock = threading.Lock()
        self.calls = []
        self.healed = False

    def size(self):
        return len(self._data)

    def _log(self, ranges):
        with self._lock:
            self.calls.append(list(ranges))
            return len(self.calls)

    def read_ranges(self, ranges):
        call = self._log(ranges)
        if self._delay:
            time.sleep(self._delay)
        if call == self._fail_on_call and not self.healed:
            raise OSError("transport failure")
        chunks = [self._data[offset:offset + size] for offset, size in ranges]
        if self._short:
            chunks[-1] = chunks[-1][:-1]
        if self._missing_chunk:
            chunks = chunks[:-1]
        if self._strided:
            chunks = [_strided_view(c) for c in chunks]
        return chunks
