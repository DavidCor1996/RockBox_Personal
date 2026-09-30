"""Read original 8-Bit Rebellion resources without running the iOS binary."""

import io
import struct
import zlib
from PIL import Image


def png_image(data):
    """Normalize Apple's raw-deflate, premultiplied BGRA CgBI PNGs."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG')
    pos, chunks, apple = 8, [], False
    while pos < len(data):
        size = struct.unpack_from('>I', data, pos)[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + size]
        if len(body) != size:
            raise ValueError('Truncated PNG')
        apple |= kind == b'CgBI'
        chunks.append((kind, body))
        pos += size + 12
    if not apple:
        return Image.open(io.BytesIO(data)).convert('RGBA')
    packed = b''.join(body for kind, body in chunks if kind == b'IDAT')
    packed = zlib.compress(zlib.decompress(packed, -15))
    output = bytearray(data[:8])
    for kind, body in chunks:
        if kind in (b'CgBI', b'IDAT'):
            continue
        if kind == b'IEND':
            output += _chunk(b'IDAT', packed)
        output += _chunk(kind, body)
    im = Image.open(io.BytesIO(output)).convert('RGBA')
    import numpy as np
    pixels = np.array(im, dtype=np.uint16)
    rgb = pixels[:, :, [2, 1, 0]]
    alpha = pixels[:, :, 3:4]
    pixels[:, :, :3] = np.minimum(255, (rgb * 255 + alpha // 2)
                                // np.maximum(alpha, 1))
    return Image.fromarray(pixels.astype('uint8'))


def _chunk(kind, body):
    return (struct.pack('>I', len(body)) + kind + body
            + struct.pack('>I', zlib.crc32(kind + body)))


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, fmt):
        size = struct.calcsize('>' + fmt)
        if self.pos + size > len(self.data):
            raise ValueError('Truncated ANU')
        value = struct.unpack_from('>' + fmt, self.data, self.pos)
        self.pos += size
        return value[0] if len(value) == 1 else value


def anu_data(data):
    """Decode MotionWelder sequence/frame/image tables; preserve type flags."""
    r = Reader(data)
    if r.take('B') != 8:
        raise ValueError('Unsupported ANU encoding')
    r.take('B')
    version_len = r.take('H')
    if version_len != 3 or data[r.pos:r.pos + 3] != b'1.0':
        raise ValueError('Unsupported ANU version')
    r.pos += 3
    sequences = [r.take('HH') for _ in range(r.take('B'))]
    frames = [r.take('HBhh') for _ in range(r.take('H'))]
    r.take('H')  # aggregate component record bytes
    groups = [[r.take('HhhB') for _ in range(r.take('H'))]
              for _ in range(r.take('H'))]
    count = r.take('H')
    images = []
    for sheet in range(r.take('B')):
        for _ in range(r.take('H')):
            images.append((sheet, *r.take('HHHH')))
    if count != len(images):
        raise ValueError('ANU image count mismatch')
    return dict(sequences=sequences, frames=frames, groups=groups,
                images=images, trailing=data[r.pos:].hex())
