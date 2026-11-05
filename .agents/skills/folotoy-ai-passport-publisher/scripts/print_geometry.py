"""Bounded, non-executing inspection and previews of submitted print geometry.

Previews are generated from the downloadable geometry, never a creator's picture.
Native engineering files remain attachments for a human reviewer to open safely.
"""
from __future__ import annotations

from array import array
import hashlib
import io
import json
import math
from pathlib import PurePosixPath
import re
import stat
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

MAX_TRIANGLES = 1_000_000
MAX_EXPANDED = 200 * 1024 * 1024
MAX_ENTRIES = 100
REFERENCE_SHA256 = "7c1d9443fd0545bf1f775a89e411b82246a68585eb92b6fbf73e3872101cd956"
ALLOWED = {".stl", ".obj", ".3mf", ".step", ".stp", ".blend", ".fbx", ".glb", ".mtl", ".txt", ".md", ".json", ".png", ".jpg", ".jpeg", ".webp"}


class ModelFileError(ValueError):
    pass


def archive_entries(raw, *, three_mf=False, budget=None):
    try:
        archive = zipfile.ZipFile(io.BytesIO(raw))
        entries = archive.infolist()
        expanded = sum(x.file_size for x in entries)
        if budget is not None:
            budget[0] += expanded
        if len(entries) > MAX_ENTRIES or expanded > MAX_EXPANDED or (budget and budget[0] > MAX_EXPANDED):
            raise ModelFileError("压缩包展开后过大或文件过多")
        names = set()
        result = []
        for item in entries:
            name = item.filename.replace("\\", "/")
            path = PurePosixPath(name)
            if path.is_absolute() or ".." in path.parts or ":" in name or any(ord(c) < 32 for c in name):
                raise ModelFileError("压缩包含不安全的文件路径")
            if stat.S_ISLNK(item.external_attr >> 16) or item.flag_bits & 1:
                raise ModelFileError("不支持符号链接或加密压缩包")
            if item.is_dir():
                continue
            if name.casefold() in names:
                raise ModelFileError("压缩包含重复文件名")
            names.add(name.casefold())
            ext = path.suffix.lower()
            allowed = ALLOWED | ({".model", ".xml", ".rels"} if three_mf else set())
            if ext not in allowed or ext == ".3mf" and three_mf:
                raise ModelFileError(f"压缩包含不支持的文件：{name}")
            data = archive.read(item)
            if hashlib.sha256(data).hexdigest() == REFERENCE_SHA256:
                raise ModelFileError("请只上传自己的设计，移除官方 AI Passport 参考模型")
            result.append((name, data))
        return result
    except (zipfile.BadZipFile, RuntimeError, OSError, NotImplementedError, EOFError) as exc:
        raise ModelFileError("压缩包无法读取") from exc


def triangle(out, points):
    if len(out) // 9 >= MAX_TRIANGLES:
        raise ModelFileError("模型面数过多，请简化到 100 万个三角面以内")
    flat = [float(value) for point in points for value in point]
    if len(flat) != 9 or any(not math.isfinite(v) or abs(v) > 1_000_000 for v in flat):
        raise ModelFileError("模型包含无效或超出范围的坐标")
    out.extend(flat)


def stl_mesh(raw):
    out = array("f")
    count = struct.unpack_from("<I", raw, 80)[0] if len(raw) >= 84 else 0
    if count and len(raw) == 84 + count * 50:
        if count > MAX_TRIANGLES:
            raise ModelFileError("模型面数过多，请简化后上传")
        for offset in range(84, len(raw), 50):
            values = struct.unpack_from("<9f", raw, offset + 12)
            triangle(out, (values[:3], values[3:6], values[6:]))
    else:
        try:
            text = raw.decode("ascii")
            if not text.lstrip().startswith("solid") or "endsolid" not in text:
                raise ValueError()
            points = []
            for line in text.splitlines():
                parts = line.split()
                if parts and parts[0] == "vertex":
                    if len(parts) != 4:
                        raise ValueError()
                    points.append(tuple(map(float, parts[1:])))
                    if len(points) == 3:
                        triangle(out, points)
                        points = []
            if points:
                raise ValueError()
        except (UnicodeDecodeError, ValueError) as exc:
            raise ModelFileError("STL 无法读取，请重新导出") from exc
    return out


def obj_mesh(raw):
    points = []
    out = array("f")
    try:
        for line in raw.decode("utf-8-sig").splitlines():
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "v":
                if len(points) >= 2_000_000 or len(parts) < 4:
                    raise ValueError()
                points.append(tuple(map(float, parts[1:4])))
            elif parts[0] == "f":
                if len(parts) != 4:
                    raise ValueError()
                indices = [int(x.split("/")[0]) for x in parts[1:]]
                if any(x == 0 or x > len(points) or x < -len(points) for x in indices):
                    raise ValueError()
                face = [points[x - 1 if x > 0 else x] for x in indices]
                for i in range(1, len(face) - 1):
                    triangle(out, (face[0], face[i], face[i + 1]))
    except (UnicodeDecodeError, ValueError, IndexError) as exc:
        raise ModelFileError("OBJ 无法读取，请导出为三角网格") from exc
    return out


def three_mf_mesh(raw, budget=None):
    entries = archive_entries(raw, three_mf=True, budget=budget)
    models = [(name, data) for name, data in entries if name.lower().endswith(".model")]
    if len(models) != 1:
        raise ModelFileError("此 3MF 使用多模型扩展，请从切片软件另导出 STL")
    data = models[0][1]
    try:
        xml_text = data.decode("utf-8-sig")
    except UnicodeError as exc:
        raise ModelFileError("3MF 必须使用 UTF-8 XML，请重新导出") from exc
    if len(data) > 80 * 1024 * 1024 or re.search(r"<!\s*(DOCTYPE|ENTITY)", xml_text, re.I) or "\x00" in xml_text:
        raise ModelFileError("3MF XML 含不支持的声明或过大")
    out = array("f")
    try:
        root = ET.fromstring(xml_text)
        if root.tag != "{http://schemas.microsoft.com/3dmanufacturing/core/2015/02}model" or root.get("requiredextensions"):
            raise ValueError()
        units = {"micron": .001, "millimeter": 1, "centimeter": 10, "inch": 25.4, "foot": 304.8, "meter": 1000}
        scale = units[root.get("unit", "millimeter")]
        objects = {o.attrib["id"]: o for o in root.findall("{*}resources/{*}object")}

        def transform(point, text):
            if not text:
                return point
            values = list(map(float, text.split()))
            if len(values) != 12 or not all(math.isfinite(x) for x in values):
                raise ValueError()
            return tuple(sum(point[j] * values[j * 3 + i] for j in range(3)) + values[9 + i] for i in range(3))

        def visit(object_id, transforms, ancestors):
            if object_id in ancestors or len(ancestors) >= 16:
                raise ValueError()
            obj = objects[object_id]
            mesh = obj.find("{*}mesh")
            if mesh is not None:
                vertices = mesh.findall("{*}vertices/{*}vertex")
                if len(vertices) > 2_000_000:
                    raise ValueError()
                points = [tuple(float(v.attrib[k]) for k in ("x", "y", "z")) for v in vertices]
                for tri in mesh.findall("{*}triangles/{*}triangle"):
                    face = []
                    for key in ("v1", "v2", "v3"):
                        idx = int(tri.attrib[key])
                        if not 0 <= idx < len(points):
                            raise ValueError()
                        point = points[idx]
                        for trans in transforms:
                            point = transform(point, trans)
                        face.append(tuple(v * scale for v in point))
                    triangle(out, face)
            for component in obj.findall("{*}components/{*}component"):
                if any(key.rsplit("}", 1)[-1] == "path" for key in component.attrib):
                    raise ValueError()
                visit(component.attrib["objectid"], [component.get("transform")] + transforms, ancestors | {object_id})
        build = root.findall("{*}build/{*}item")
        if not build:
            raise ValueError()
        for item in build:
            if any(key.rsplit("}", 1)[-1] == "path" for key in item.attrib):
                raise ValueError()
            if item.get("printable", "1") not in ("0", "false"):
                visit(item.attrib["objectid"], [item.get("transform")], set())
    except (ET.ParseError, ValueError, KeyError, IndexError, OverflowError) as exc:
        raise ModelFileError("此 3MF 结构暂不支持，请另导出 STL") from exc
    return out


def mesh_glb(points):
    if not points:
        raise ModelFileError("模型没有可打印的几何面")
    mins = [min(points[i::3]) for i in range(3)]
    maxs = [max(points[i::3]) for i in range(3)]
    sizes = [maxs[i] - mins[i] for i in range(3)]
    if any(not math.isfinite(v) or v <= 0 for v in sizes) or max(sizes) > 10_000:
        raise ModelFileError("模型尺寸无效，请用毫米导出有厚度的实体")
    normals = array("f")
    degenerate = 0
    for i in range(0, len(points), 9):
        a = [points[i + 3 + j] - points[i + j] for j in range(3)]
        b = [points[i + 6 + j] - points[i + j] for j in range(3)]
        n = [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
        length = math.sqrt(sum(v*v for v in n))
        if length < 1e-12:
            degenerate += 1
        n = [v / length for v in n] if length else [0, 0, 1]
        normals.extend(n * 3)
    if degenerate == len(points) // 9:
        raise ModelFileError("模型全部为退化面，请修复后再上传")
    # glTF uses metres and Y up; print inputs use millimetres and Z up.
    positions = array("f")
    rotated_normals = array("f")
    for i in range(0, len(points), 3):
        positions.extend((points[i]*.001, points[i+2]*.001, -points[i+1]*.001))
        rotated_normals.extend((normals[i], normals[i+2], -normals[i+1]))
    if sys.byteorder != "little":
        positions.byteswap(); rotated_normals.byteswap()
    payload = positions.tobytes() + rotated_normals.tobytes()
    count = len(points) // 3
    doc = {"asset": {"version": "2.0", "generator": "AI Passport geometry preview"}, "scene": 0,
           "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "material": 0}]}],
           "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [.3, .65, .55, 1], "metallicFactor": 0, "roughnessFactor": .65}, "doubleSided": True}],
           "buffers": [{"byteLength": len(payload)}],
           "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(positions)*4}, {"buffer": 0, "byteOffset": len(positions)*4, "byteLength": len(rotated_normals)*4}],
           "accessors": [{"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3", "min": [mins[0]*.001, mins[2]*.001, -maxs[1]*.001], "max": [maxs[0]*.001, maxs[2]*.001, -mins[1]*.001]}, {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"}]}
    header = json.dumps(doc, separators=(",", ":")).encode()
    header += b" " * (-len(header) % 4)
    glb = struct.pack("<III", 0x46546C67, 2, 12+8+len(header)+8+len(payload)) + struct.pack("<I4s", len(header), b"JSON") + header + struct.pack("<I4s", len(payload), b"BIN\0") + payload
    return glb, {"triangles": len(points)//9, "dimensionsMm": [round(v, 4) for v in sizes], "degenerateFaces": degenerate}


def inspect_model(raw, name, _budget=None):
    if hashlib.sha256(raw).hexdigest() == REFERENCE_SHA256:
        raise ModelFileError("请只上传自己的设计，移除官方 AI Passport 参考模型")
    ext = PurePosixPath(name).suffix.lower()
    budget = _budget if _budget is not None else [0]
    readers = {".stl": stl_mesh, ".obj": obj_mesh, ".3mf": lambda data: three_mf_mesh(data, budget)}
    report = {"format": ext[1:], "entries": [], "requiresManualReview": True}
    if ext in readers:
        glb, info = mesh_glb(readers[ext](raw))
        return {**report, **info, "printableGeometry": True}, glb
    if ext == ".zip":
        meshes = array("f")
        for entry_name, data in archive_entries(raw, budget=budget):
            entry_ext = PurePosixPath(entry_name).suffix.lower()
            if entry_ext in readers:
                part = readers[entry_ext](data)
                if len(meshes) + len(part) > MAX_TRIANGLES * 9:
                    raise ModelFileError("压缩包模型面数过多，请简化后上传")
                meshes.extend(part)
                entry_report = {"format": entry_ext[1:], "triangles": len(part) // 9}
            else:
                entry_report, _ = inspect_model(data, entry_name, budget)
            report["entries"].append({"name": entry_name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(), **entry_report})
        if meshes:
            glb, info = mesh_glb(meshes)
            return {**report, **info, "printableGeometry": True}, glb
        return report, None
    if ext == ".blend" and not raw.startswith(b"BLENDER"):
        raise ModelFileError("请保存为未压缩的 Blender 工程")
    if ext == ".fbx" and not (raw.startswith(b"Kaydara FBX Binary") or b"FBXHeaderExtension" in raw[:4096]):
        raise ModelFileError("FBX 文件无法识别")
    if ext in {".step", ".stp"} and not raw.lstrip().startswith(b"ISO-10303-21;"):
        raise ModelFileError("STEP 文件无法识别")
    if ext == ".glb" and (len(raw) < 20 or raw[:4] != b"glTF" or struct.unpack_from("<II", raw, 4) != (2, len(raw))):
        raise ModelFileError("GLB 文件无法识别")
    if ext not in ALLOWED:
        raise ModelFileError("不支持此附件格式")
    return report, None
