#!/usr/bin/env python3
"""Smoke tests for the voxkit command-line voxelizer."""

from __future__ import annotations

import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile


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


def write_binary_stl_cubes(path: Path, offsets: list[float]) -> None:
    triangles = []
    faces = (
        (0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
        (1, 5, 6, 2), (2, 6, 7, 3), (4, 0, 3, 7),
    )
    for offset in offsets:
        points = [
            (offset + 0, 0, 0), (offset + 1, 0, 0), (offset + 1, 1, 0), (offset + 0, 1, 0),
            (offset + 0, 0, 1), (offset + 1, 0, 1), (offset + 1, 1, 1), (offset + 0, 1, 1),
        ]
        for a, b, c, d in faces:
            triangles.extend(((points[a], points[b], points[c]), (points[a], points[c], points[d])))
    header = b"two independent cube shells".ljust(80, b" ")
    records = bytearray()
    for triangle in triangles:
        records.extend(struct.pack("<3f", 0.0, 0.0, 0.0))
        records.extend(struct.pack("<9f", *(coordinate for point in triangle for coordinate in point)))
        records.extend(struct.pack("<H", 0))
    path.write_bytes(header + struct.pack("<I", len(triangles)) + records)


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


def write_binvox(path: Path, dimensions: int, occupied: set[tuple[int, int, int]]) -> None:
    values = bytearray(dimensions**3)
    for x, y, z in occupied:
        values[x + y * dimensions + z * dimensions**2] = 1
    runs: list[tuple[int, int]] = []
    current = values[0]
    count = 0
    for value in values:
        if value == current and count < 255:
            count += 1
        else:
            runs.append((current, count))
            current = value
            count = 1
    runs.append((current, count))
    header = (
        f"#binvox 1\ndim {dimensions} {dimensions} {dimensions}\n"
        f"translate 0 0 0\nscale {dimensions}\nvoxel_mode pixel\ndata\n"
    ).encode("ascii")
    path.write_bytes(header + b"".join(bytes(run) for run in runs))


def stl_bounds(path: Path) -> tuple[tuple[float, float], ...]:
    data = path.read_bytes()
    points: list[tuple[float, float, float]] = []
    if len(data) >= 84:
        triangle_count = struct.unpack_from("<I", data, 80)[0]
        if 84 + triangle_count * 50 == len(data):
            for triangle in range(triangle_count):
                offset = 84 + triangle * 50 + 12
                for vertex in range(3):
                    points.append(struct.unpack_from("<3f", data, offset + vertex * 12))
    if not points:
        for line in data.decode("ascii").splitlines():
            fields = line.split()
            if fields and fields[0].lower() == "vertex":
                points.append(tuple(float(value) for value in fields[1:4]))
    assert points
    return tuple((min(point[axis] for point in points), max(point[axis] for point in points)) for axis in range(3))


def stl_shell_count(path: Path) -> int:
    data = path.read_bytes()
    triangle_count = struct.unpack_from("<I", data, 80)[0] if len(data) >= 84 else 0
    triangles: list[list[tuple[float, ...]]] = []
    if len(data) == 84 + triangle_count * 50:
        for triangle in range(triangle_count):
            offset = 84 + triangle * 50 + 12
            triangles.append([
                tuple(round(value, 5) for value in struct.unpack_from("<3f", data, offset + vertex * 12))
                for vertex in range(3)
            ])
    else:
        current: list[tuple[float, ...]] = []
        for line in data.decode("ascii").splitlines():
            fields = line.split()
            if fields and fields[0].lower() == "vertex":
                current.append(tuple(round(float(value), 5) for value in fields[1:4]))
                if len(current) == 3:
                    triangles.append(current)
                    current = []
    triangle_count = len(triangles)
    edge_owner: dict[tuple[tuple[float, ...], tuple[float, ...]], int] = {}
    parent = list(range(triangle_count))

    def root(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    for triangle, points in enumerate(triangles):
        for edge in range(3):
            endpoints = tuple(sorted((points[edge], points[(edge + 1) % 3])))
            owner = edge_owner.get(endpoints)
            if owner is None:
                edge_owner[endpoints] = triangle
            else:
                left, right = root(triangle), root(owner)
                if left != right:
                    parent[right] = left
    return len({root(index) for index in range(triangle_count)})


def read_glb_document(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF" and version == 2 and total_length == len(data)
    json_length, json_type = struct.unpack_from("<II", data, 12)
    assert json_type == 0x4E4F534A
    return json.loads(data[20 : 20 + json_length].decode("utf-8"))


def three_mf_bounds(path: Path) -> tuple[tuple[float, float], ...]:
    with zipfile.ZipFile(path) as archive:
        model = ET.fromstring(archive.read("3D/3dmodel.model"))
    vertices = model.findall(".//{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}vertex")
    points = [tuple(float(vertex.attrib[axis]) for axis in ("x", "y", "z")) for vertex in vertices]
    assert points
    return tuple((min(point[axis] for point in points), max(point[axis] for point in points)) for axis in range(3))


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

        thin_pixel_grid = root / "thin-pixel.binvox"
        original_stl = root / "thin-original.stl"
        compensated_stl = root / "thin-compensated.stl"
        scaled_compensated_stl = root / "thin-scaled-compensated.stl"
        scaled_compensated_3mf = root / "thin-scaled-compensated.3mf"
        write_binvox(thin_pixel_grid, 16, {(8, 8, z) for z in range(4, 12)})
        result = subprocess.run(
            [str(binary), "--export-stl", str(thin_pixel_grid), "-o", str(original_stl)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        result = subprocess.run(
            [str(binary), "--export-stl", str(thin_pixel_grid), "-o", str(compensated_stl), "--minimum-feature-width-mm", "3"],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "expandedVoxels=82" in result.stdout
        original_bounds = stl_bounds(original_stl)
        compensated_bounds = stl_bounds(compensated_stl)
        assert original_bounds[0] == (8.0, 9.0) and original_bounds[1] == (8.0, 9.0)
        assert compensated_bounds[0] == (7.0, 10.0) and compensated_bounds[1] == (7.0, 10.0)
        assert compensated_bounds[2] == (3.0, 13.0)
        result = subprocess.run(
            [str(binary), "--export-stl", str(thin_pixel_grid), "-o", str(scaled_compensated_stl), "--minimum-feature-width-mm", "3", "--scale", "2"],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert stl_bounds(scaled_compensated_stl) == ((14.0, 20.0), (14.0, 20.0), (6.0, 26.0))
        result = subprocess.run(
            [str(binary), "--export-3mf", str(thin_pixel_grid), "-o", str(scaled_compensated_3mf), "--minimum-feature-width-mm", "3", "--scale", "2"],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert three_mf_bounds(scaled_compensated_3mf) == ((14.0, 20.0), (14.0, 20.0), (6.0, 26.0))

        multi_part_stl = root / "two-parts.stl"
        multi_part_binvox = root / "two-parts.binvox"
        multi_part_3mf = root / "two-parts.3mf"
        multi_part_obj = root / "two-parts.obj"
        multi_part_glb = root / "two-parts.glb"
        multi_part_output_stl = root / "two-parts-output.stl"
        write_binary_stl_cubes(multi_part_stl, [0.0, 1.02])
        result = subprocess.run(
            [str(binary), str(multi_part_stl), "-r", "24", "--voxel-mode", "pixel", "--split-parts", "-o", str(multi_part_binvox)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "splitParts=true components=2" in result.stdout
        part_sidecar = Path(f"{multi_part_binvox}.parts")
        assert part_sidecar.exists() and part_sidecar.stat().st_size > 28
        for format_name, output_path in (("3mf", multi_part_3mf), ("obj", multi_part_obj), ("glb", multi_part_glb)):
            result = subprocess.run(
                [str(binary), f"--export-{format_name}", str(multi_part_binvox), "-o", str(output_path)],
                text=True, capture_output=True, check=False,
            )
            assert result.returncode == 0, result.stderr
        with zipfile.ZipFile(multi_part_3mf) as archive:
            model = ET.fromstring(archive.read("3D/3dmodel.model"))
        namespace = "{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}"
        objects = model.findall(f".//{namespace}resources/{namespace}object")
        assert len(objects) == 2
        assert [obj.attrib["name"] for obj in objects] == ["VoxelPart_1", "VoxelPart_2"]
        assert len(model.findall(f".//{namespace}build/{namespace}item")) == 2
        assert multi_part_obj.read_text().count("\no VoxelPart_") == 2
        glb_document = read_glb_document(multi_part_glb)
        assert len(glb_document["nodes"]) == 2
        assert glb_document["scenes"][0]["nodes"] == [0, 1]
        assert [node["name"] for node in glb_document["nodes"]] == ["VoxelPart_1", "VoxelPart_2"]

        separated_part_stl = root / "two-separated-parts.stl"
        separated_part_binvox = root / "two-separated-parts.binvox"
        write_binary_stl_cubes(multi_part_stl, [0.0, 1.25])
        result = subprocess.run(
            [str(binary), str(multi_part_stl), "-r", "24", "--voxel-mode", "pixel", "--split-parts", "-o", str(separated_part_binvox)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        result = subprocess.run(
            [str(binary), "--export-stl", str(separated_part_binvox), "-o", str(multi_part_output_stl)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        # Surface voxelization produces an inner and outer boundary shell for
        # each source cube. The four shells must remain separated across the
        # gap between the two original parts.
        assert stl_shell_count(multi_part_output_stl) == 4
        assert sum(line.startswith("solid VoxelPart_") for line in multi_part_output_stl.read_text(encoding="ascii").splitlines()) == 2

        # Coincident faces are how several CAD-to-STL exporters encode bodies
        # that touch.  They must remain separate source parts before
        # voxelization instead of collapsing into one connected shell.
        touching_parts_stl = root / "sixteen-touching-parts.stl"
        touching_parts_binvox = root / "sixteen-touching-parts.binvox"
        touching_parts_output = root / "sixteen-touching-parts-output.stl"
        write_binary_stl_cubes(touching_parts_stl, [float(index) for index in range(16)])
        result = subprocess.run(
            [str(binary), str(touching_parts_stl), "-r", "64", "--voxel-mode", "pixel", "--split-parts", "-o", str(touching_parts_binvox)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert "splitParts=true components=16" in result.stdout
        result = subprocess.run(
            [str(binary), "--export-stl", str(touching_parts_binvox), "-o", str(touching_parts_output)],
            text=True, capture_output=True, check=False,
        )
        assert result.returncode == 0, result.stderr
        assert touching_parts_output.stat().st_size > 84

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
