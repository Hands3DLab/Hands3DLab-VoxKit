#!/usr/bin/env python3
"""Smoke tests for the voxkit command-line voxelizer."""

from __future__ import annotations

import os
from pathlib import Path
import struct
import subprocess
import tempfile


CUBE_OBJ = """# unit cube
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 0 0 1
v 1 0 1
v 1 1 1
v 0 1 1
f 1 2 3 4
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 5 1 4 8
"""

ASCII_STL = """solid triangle
    facet normal 0 0 1
        outer loop
            vertex 0 0 0
            vertex 1 0 0
            vertex 0 1 0
        endloop
    endfacet
endsolid triangle
"""


def write_binary_stl(path: Path) -> None:
        header = b"binary triangle".ljust(80, b" ")
        normal = struct.pack("<3f", 0.0, 0.0, 1.0)
        vertices = struct.pack("<9f", 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
        path.write_bytes(header + struct.pack("<I", 1) + normal + vertices + struct.pack("<H", 0))


def read_binvox(path: Path) -> tuple[tuple[int, int, int], int]:
    with path.open("rb") as stream:
        header = stream.readline()
        assert header == b"#binvox 1\n"
        dims: tuple[int, int, int] | None = None
        while True:
            line = stream.readline()
            assert line
            if line == b"data\n":
                break
            if line.startswith(b"dim "):
                parsed = tuple(int(value) for value in line.split()[1:])
                assert len(parsed) == 3
                dims = parsed
        assert dims is not None
        payload = stream.read()

    values: list[int] = []
    for value, count in struct.iter_unpack("BB", payload):
        values.extend([value] * count)
    assert len(values) == dims[0] * dims[1] * dims[2]
    return dims, sum(values)


def main() -> int:
    binary = Path(os.environ["VOXKIT_BIN"])
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source = root / "cube.obj"
        output = root / "cube.binvox"
        source.write_text(CUBE_OBJ, encoding="utf-8")

        result = subprocess.run(
            [str(binary), str(source), "--resolution", "16", "--output", str(output)],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "H3DL_PROGRESS 100 done" in result.stdout
        dimensions, occupied = read_binvox(output)
        assert dimensions == (16, 16, 16)
        assert occupied > 0

        topology: dict[str, tuple[int, int]] = {}
        modes = ("pixel", "triangle", "quad")
        if os.environ.get("VOXKIT_SKIP_GPU_TEST") == "1" or os.name == "nt":
            modes = ("pixel", "quad")
        for mode in modes:
            mode_binvox = root / f"cube-{mode}.binvox"
            mode_obj = root / f"cube-{mode}.obj"
            result = subprocess.run(
                [str(binary), str(source), "-r", "16", "--voxel-mode", mode, "-o", str(mode_binvox)],
                text=True, capture_output=True, check=False,
            )
            assert result.returncode == 0, result.stderr
            result = subprocess.run(
                [str(binary), "--export-obj", str(mode_binvox), "-o", str(mode_obj)],
                text=True, capture_output=True, check=False,
            )
            assert result.returncode == 0, result.stderr
            face_sizes = [len(line.split()) - 1 for line in mode_obj.read_text().splitlines() if line.startswith("f ")]
            topology[mode] = (face_sizes.count(3), face_sizes.count(4))
        assert topology["pixel"][0] > 0 and topology["pixel"][1] == 0
        if "triangle" in topology:
            assert topology["triangle"][0] > 0 and topology["triangle"][1] == 0
        assert topology["quad"][0] == 0 and topology["quad"][1] > 0

        print_output = root / "cube-snapmaker-u1.stl"
        result = subprocess.run(
            [str(binary), "--export-stl", str(output), "--output", str(print_output)],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "H3DL_PROGRESS 100 done" in result.stdout
        stl = print_output.read_bytes()
        assert len(stl) >= 84
        triangle_count = struct.unpack_from("<I", stl, 80)[0]
        assert triangle_count > 0
        assert len(stl) == 84 + triangle_count * 50

        ascii_source = root / "triangle.stl"
        ascii_output = root / "triangle-ascii.binvox"
        ascii_source.write_text(ASCII_STL, encoding="ascii")
        result = subprocess.run(
            [str(binary), str(ascii_source), "-r", "8", "-o", str(ascii_output)],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert read_binvox(ascii_output)[1] > 0

        binary_source = root / "triangle-binary.stl"
        binary_output = root / "triangle-binary.binvox"
        write_binary_stl(binary_source)
        result = subprocess.run(
            [str(binary), str(binary_source), "-r", "8", "-o", str(binary_output)],
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 0, result.stderr
        assert read_binvox(binary_output)[1] > 0

        invalid = subprocess.run(
            [str(binary), str(source), "--resolution", "0"],
            text=True,
            capture_output=True,
            check=False,
        )
        assert invalid.returncode != 0
        assert "resolution must be in 1..2048" in invalid.stderr

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
