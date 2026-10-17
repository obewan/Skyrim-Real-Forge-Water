"""Removes the compression blocks from the extracted wave texture and saves it as BC7.

The vanilla wave texture is a DXT1 normal map: its 4x4 blocks and 5-6 bit colors show up as jagged
highlights on a small water surface like a forge trough, which shows only a small part of it. This
script smooths those blocks away (the waves themselves are much larger, so they are kept), makes the
normals unit length again and saves the result as BC7, which keeps 8 bits per channel.

    py tools\\extract_wave_texture.py "C:\\...\\Skyrim Special Edition\\Data"
    py tools\\smooth_wave_texture.py "C:\\...\\texconv.exe"

Needs numpy and Pillow (py -m pip install numpy pillow) and texconv (github.com/microsoft/DirectXTex).
"""

import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

TEXTURE = os.path.join(os.path.dirname(__file__), "..", "data", "Textures", "RealForgeWater", "Water", "DefaultWater.dds")
# Blur radius in pixels: enough to hide the 4x4 compression blocks, far below the size of the waves.
SIGMA = 1.5


def texconv(exe, *args):
    subprocess.run([exe, "-nologo", "-y", *args], check=True, stdout=subprocess.DEVNULL)


def tiled_blur(channels, sigma):
    """Gaussian blur that wraps around the edges, so the texture still tiles without a seam."""
    height, width = channels.shape[:2]
    fy = np.fft.fftfreq(height)[:, None]
    fx = np.fft.fftfreq(width)[None, :]
    kernel = np.exp(-2 * (np.pi * sigma) ** 2 * (fx**2 + fy**2))
    spectrum = np.fft.fft2(channels, axes=(0, 1)) * kernel[..., None]
    return np.fft.ifft2(spectrum, axes=(0, 1)).real


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    exe = sys.argv[1]
    texture = os.path.normpath(TEXTURE)
    if not os.path.exists(texture):
        sys.exit(f"{texture} not found: run tools\\extract_wave_texture.py first.")
    with open(texture, "rb") as dds:
        if dds.read(88)[84:88] != b"DXT1":
            sys.exit(f"{texture} is not the vanilla DXT1 texture (already smoothed?): extract it again first.")

    with tempfile.TemporaryDirectory() as work:
        texconv(exe, "-ft", "png", "-m", "1", "-o", work, texture)
        png = os.path.join(work, "DefaultWater.png")
        rgb = np.asarray(Image.open(png).convert("RGB"), dtype=np.float64)

        normals = tiled_blur(rgb / 255 * 2 - 1, SIGMA)
        normals /= np.linalg.norm(normals, axis=2, keepdims=True)
        rgb = np.clip(np.rint((normals + 1) / 2 * 255), 0, 255).astype(np.uint8)
        Image.fromarray(rgb, "RGB").save(png)

        texconv(exe, "-f", "BC7_UNORM", "-bc", "x", "-m", "0", "-o", os.path.dirname(texture), png)
    print(f"wrote {texture} (BC7)")


if __name__ == "__main__":
    main()
