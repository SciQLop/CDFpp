import io


class S3object:
    """An S3 object that reads like a file: read fetches a byte range from the current position."""

    def __init__(self, fhandle):
        self.pos = 0
        self.content_length = fhandle.content_length
        self.fhandle = fhandle
        self.temp_file = None

    def read(self, isize):
        size = self.content_length if isize == -1 else isize
        byte_range = f"bytes={self.pos}-{self.pos + size - 1}"
        self.pos += size
        return self.fhandle.get(Range=byte_range)["Body"].read()

    def seek(self, offset, from_what=0):
        origin = {0: 0, 1: self.pos, 2: self.content_length}[from_what]
        self.pos = origin + offset

    def tell(self):
        return self.pos

    def fetchS3entire(self):
        return io.BytesIO(self.fhandle.get()["Body"].read())
