#!/usr/bin/env python3
"""Generate deterministic PPM inputs without third-party Python packages."""

from pathlib import Path

WIDTH = 320
HEIGHT = 240
COUNT = 20


def pixel(frame: int, x: int, y: int) -> bytes:
    r = (x + frame * 29) % 256
    g = (y * 2 + frame * 47) % 256
    b = ((x // 8 + y // 8) * 19 + frame * 71) % 256
    return bytes((r, g, b))


def main() -> None:
    assets = Path(__file__).resolve().parents[1] / "assets"
    assets.mkdir(exist_ok=True)
    for frame in range(COUNT):
        path = assets / f"{frame:03}.ppm"
        with path.open("wb") as output:
            output.write(f"P6\n{WIDTH} {HEIGHT}\n255\n".encode("ascii"))
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    output.write(pixel(frame, x, y))
    print(f"Generated {COUNT} images in {assets}")


if __name__ == "__main__":
    main()

