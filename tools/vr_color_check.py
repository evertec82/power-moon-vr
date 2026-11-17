"""Check GPU captures made with ODYSSEY_VR_COLOR_CHECK=1 during an offscreen replay."""
import argparse
from pathlib import Path

from png_stats import read_png


def rgb_pixels(path):
    width, height, channels, rows = read_png(path)
    if channels not in (3, 4):
        raise ValueError(f"Expected RGB/RGBA capture, got {channels} channels")
    pixels = b"".join(rows)
    rgb = b"".join(pixels[channel::channels] for channel in range(3))
    return (width, height), rgb


def srgb_to_linear_byte(value):
    encoded = value / 255
    linear = encoded / 12.92 if encoded <= 0.04045 else ((encoded + 0.055) / 1.055) ** 2.4
    return round(linear * 255)


def check_captures(directory):
    size, reference = rgb_pixels(directory / "vr_color_reference.png")
    corrected_size, corrected = rgb_pixels(directory / "vr_color_corrected.png")
    linear_size, linear = rgb_pixels(directory / "vr_color_linear.png")
    legacy_size, legacy = rgb_pixels(directory / "vr_color_legacy.png")
    if corrected_size != size or linear_size != size or legacy_size != size:
        raise ValueError("Capture dimensions differ")

    corrected_error = max(abs(a - b) for a, b in zip(reference, corrected))
    lookup = bytes(srgb_to_linear_byte(value) for value in range(256))
    expected_linear = reference.translate(lookup)
    linear_error = max(abs(a - b) for a, b in zip(expected_linear, linear))
    legacy_error = sum(abs(a - b) for a, b in zip(reference, legacy)) / len(reference)
    print(f"{size[0]}x{size[1]} GPU scanout comparison")
    print(f"sRGB destination maximum error {corrected_error}/255")
    print(f"Linear destination maximum error {linear_error}/255")
    print(f"Legacy blit mean absolute error {legacy_error:.3f}/255")
    if corrected_error > 1 or linear_error > 1:
        raise SystemExit("Color transfer regression")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Directory containing the four vr_color_*.png captures")
    check_captures(parser.parse_args().directory)
