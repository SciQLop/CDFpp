"""
pycdfpp._remote
---------------
Reads URLs (s3://, gs://, https://...) through fsspec, for ``pycdfpp.load``. Imported only
when a URL is loaded, so fsspec stays optional.
"""
from typing import Optional


def _aws_credentials_found() -> bool:
    try:
        import botocore.session
    except ImportError:
        return True  # s3fs is missing too: let fsspec say what to install
    return botocore.session.get_session().get_credentials() is not None


def _default_storage_options(url: str) -> dict:
    """Public S3 buckets open without setup: with no AWS credentials at all, read anonymously.
    Credentials that exist are always used, even if wrong: falling back would hide the error."""
    if url.startswith('s3://') and not _aws_credentials_found():
        return {'anon': True}
    return {}


class FsspecReader:
    """Byte-range reader over any fsspec URL: s3://, gs://, az://, https://..."""

    def __init__(self, url: str, storage_options: Optional[dict]):
        try:
            import fsspec
        except ImportError:
            raise ImportError(f"loading '{url}' needs fsspec: pip install fsspec "
                              "(plus s3fs for s3://, gcsfs for gs://...)") from None
        if storage_options is None:
            storage_options = _default_storage_options(url)
        self.fs, self.path = fsspec.core.url_to_fs(url, **storage_options)
        self._size = self.fs.size(self.path)

    def size(self):
        return self._size

    def read_ranges(self, ranges):
        return self.fs.cat_ranges([self.path] * len(ranges),
                                  [offset for offset, _ in ranges],
                                  [offset + size for offset, size in ranges])
