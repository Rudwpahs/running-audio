"""PR1 audio prototype: IMA-ADPCM block codec (host reference) + demo clip + blob tools.

Block format (one RF payload, 100 bytes; also the stored-clip format):
  0-1  block_seq   u16 little-endian
  2-3  predictor   i16 LE   decoder state before the first coded sample
  4    step_index  u8 (0..88)
  5    flags       u8 (bit0: last block of the clip)
  6-99 94 bytes = 188 4-bit codes, low nibble first
188 samples per block = 5.875 ms at 32 kHz. Every block is self-contained (state in its
header), so a lost block never desynchronises the decoder.

The codec is standard IMA-ADPCM (the August Pr1ImaAdpcm.h source was never committed and
is re-implemented here; firmware decoder: firmware/t3s3_audio_bringup/include/pr1_ima_adpcm.hpp).

  python tools/audio/pr1_adpcm.py make-demo <out.adpcm> [--wav out.wav]
  python tools/audio/pr1_adpcm.py encode <in.wav> <out.adpcm>   (any rate/channels -> 32 kHz mono)
  python tools/audio/pr1_adpcm.py decode <in.adpcm> <out.wav>
"""
import math
import struct
import sys
import wave

RATE = 32000
BLOCK_BYTES = 100
HEADER_BYTES = 6
SAMPLES_PER_BLOCK = (BLOCK_BYTES - HEADER_BYTES) * 2  # 188

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
        88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598,
        658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
        3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def decode_nibble(code, pred, idx):
    step = STEP[idx]
    diff = step >> 3
    if code & 4: diff += step
    if code & 2: diff += step >> 1
    if code & 1: diff += step >> 2
    pred = pred - diff if code & 8 else pred + diff
    pred = _clamp(pred, -32768, 32767)
    idx = _clamp(idx + INDEX[code & 7], 0, 88)
    return pred, idx


def encode_sample(sample, pred, idx):
    step = STEP[idx]
    delta = sample - pred
    code = 0
    if delta < 0:
        code = 8
        delta = -delta
    if delta >= step: code |= 4; delta -= step
    if delta >= step >> 1: code |= 2; delta -= step >> 1
    if delta >= step >> 2: code |= 1
    pred, idx = decode_nibble(code, pred, idx)  # track the decoder exactly
    return code, pred, idx


def encode(pcm):
    """pcm: list of int16 -> bytes of 100 B blocks."""
    out = bytearray()
    pred, idx = 0, 0
    nblocks = (len(pcm) + SAMPLES_PER_BLOCK - 1) // SAMPLES_PER_BLOCK
    for b in range(nblocks):
        chunk = pcm[b * SAMPLES_PER_BLOCK:(b + 1) * SAMPLES_PER_BLOCK]
        chunk = chunk + [0] * (SAMPLES_PER_BLOCK - len(chunk))
        flags = 1 if b == nblocks - 1 else 0
        hdr = struct.pack("<HhBB", b & 0xFFFF, pred, idx, flags)
        codes = []
        for s in chunk:
            c, pred, idx = encode_sample(s, pred, idx)
            codes.append(c)
        body = bytes(codes[i] | (codes[i + 1] << 4) for i in range(0, SAMPLES_PER_BLOCK, 2))
        out += hdr + body
    return bytes(out)


def decode_block(block):
    seq, pred, idx, flags = struct.unpack_from("<HhBB", block, 0)
    out = []
    for byte in block[HEADER_BYTES:BLOCK_BYTES]:
        for code in (byte & 0xF, byte >> 4):
            pred, idx = decode_nibble(code, pred, idx)
            out.append(pred)
    return seq, flags, out


def decode(blob):
    pcm = []
    for off in range(0, len(blob) - BLOCK_BYTES + 1, BLOCK_BYTES):
        pcm += decode_block(blob[off:off + BLOCK_BYTES])[2]
    return pcm


def demo_melody():
    """'Twinkle, Twinkle, Little Star' (traditional, public domain), soft piano-like synthesis."""
    C4, D4, E4, F4, G4, A4 = 261.63, 293.66, 329.63, 349.23, 392.00, 440.00
    line1 = [C4, C4, G4, G4, A4, A4, G4, None, F4, F4, E4, E4, D4, D4, C4, None]
    line2 = [G4, G4, F4, F4, E4, E4, D4, None]
    notes = line1 + line2 + line2 + line1
    beat = 0.42
    pcm = []
    for f in notes:
        n = int(beat * RATE)
        for i in range(n):
            t = i / RATE
            if f is None:
                pcm.append(0)
                continue
            env = min(1.0, t / 0.01) * math.exp(-3.2 * t)
            v = (math.sin(2 * math.pi * f * t) + 0.45 * math.sin(4 * math.pi * f * t) +
                 0.2 * math.sin(6 * math.pi * f * t) + 0.25 * math.sin(math.pi * f * t))
            pcm.append(int(round(9000 * env * v / 1.9)))
    pcm += [0] * int(0.6 * RATE)
    return pcm


def read_wav_32k_mono(path):
    with wave.open(path, "rb") as w:
        ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit("16-bit PCM WAV required")
    vals = struct.unpack("<%dh" % (len(raw) // 2), raw)
    mono = [sum(vals[i:i + ch]) // ch for i in range(0, len(vals), ch)]
    if rate == RATE:
        return mono
    out, pos, step = [], 0.0, rate / RATE  # linear-interpolation resampler
    while pos < len(mono) - 1:
        i = int(pos); fr = pos - i
        out.append(int(mono[i] * (1 - fr) + mono[i + 1] * fr))
        pos += step
    return out


def write_wav(path, pcm):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(pcm), *pcm))


def main(argv):
    if len(argv) >= 2 and argv[0] == "make-demo":
        pcm = demo_melody()
        blob = encode(pcm)
        open(argv[1], "wb").write(blob)
        if "--wav" in argv:
            write_wav(argv[argv.index("--wav") + 1], decode(blob))
        print(f"demo: {len(pcm)} samples ({len(pcm) / RATE:.1f} s), {len(blob) // BLOCK_BYTES} blocks, {len(blob)} B")
    elif len(argv) == 3 and argv[0] == "encode":
        blob = encode(read_wav_32k_mono(argv[1]))
        open(argv[2], "wb").write(blob)
        print(f"{len(blob) // BLOCK_BYTES} blocks")
    elif len(argv) == 3 and argv[0] == "decode":
        write_wav(argv[2], decode(open(argv[1], "rb").read()))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
