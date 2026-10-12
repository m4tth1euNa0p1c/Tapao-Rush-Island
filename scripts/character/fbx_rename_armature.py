"""Renames the Blender armature object of binary FBX files (T-170).

Unreal drops the armature node of a Blender FBX only when it is named exactly "Armature"; the musket pack of the second
PC (bean-musket-anim v0.2) exports "Armature.001", which Unreal keeps as an extra root bone (Armature_001): the
first person arms and the bean clips then miss the project's bean skeleton (LoadErrors "missing bones ... Armature_001").

The binary FBX is parsed and written back node by node (end offsets recomputed), only the names "<old>\\x00\\x01<Class>"
change; property arrays (compressed or not) are copied as they are.

  python scripts/character/fbx_rename_armature.py <src dir> <dst dir> [--old Armature.001] [--new Armature]
Copies every .fbx of <src dir> (recursively) to <dst dir> with the same relative path; other files are copied as is.
"""
import argparse
import os
import shutil
import struct

MAGIC = b"Kaydara FBX Binary  \x00\x1a\x00"


class Node:
	__slots__ = ("name", "props", "children", "has_block")

	def __init__(self, name, props, children, has_block):
		# has_block: the record holds a child block closed by a null record (even empty, as the SDK writes it for nodes
		# without properties)
		self.name, self.props, self.children, self.has_block = name, props, children, has_block


def read_nodes(data, pos, end, wide):
	nodes = []
	head = 25 if wide else 13
	while pos < end:
		if wide:
			end_offset, num_props, props_len = struct.unpack_from("<QQQ", data, pos)
		else:
			end_offset, num_props, props_len = struct.unpack_from("<III", data, pos)
		name_len = data[pos + head - 1]
		if end_offset == 0:
			return nodes, pos + head
		name = data[pos + head:pos + head + name_len]
		props_start = pos + head + name_len
		props = parse_props(data, props_start, num_props)
		child_start = props_start + props_len
		children = []
		has_block = child_start < end_offset
		if has_block:
			children, _ = read_nodes(data, child_start, end_offset, wide)
		nodes.append(Node(name, props, children, has_block))
		pos = end_offset
	return nodes, pos


def parse_props(data, pos, count):
	"""Each property as (type code, raw bytes after the code); strings are decoded to allow renaming."""
	props = []
	sizes = {b"Y": 2, b"C": 1, b"I": 4, b"F": 4, b"D": 8, b"L": 8}
	for _ in range(count):
		code = data[pos:pos + 1]
		pos += 1
		if code in sizes:
			n = sizes[code]
			props.append((code, data[pos:pos + n]))
			pos += n
		elif code in (b"S", b"R"):
			(n,) = struct.unpack_from("<I", data, pos)
			props.append((code, data[pos + 4:pos + 4 + n]))
			pos += 4 + n
		elif code in (b"f", b"d", b"l", b"i", b"b", b"c"):
			_, _, comp_len = struct.unpack_from("<III", data, pos)
			props.append((code, data[pos:pos + 12 + comp_len]))
			pos += 12 + comp_len
		else:
			raise ValueError(f"unknown FBX property type {code!r} at {pos}")
	return props


def write_props(props):
	out = bytearray()
	for code, raw in props:
		out += code
		if code in (b"S", b"R"):
			out += struct.pack("<I", len(raw)) + raw
		else:
			out += raw
	return bytes(out)


def write_nodes(nodes, offset, wide):
	out = bytearray()
	head = 25 if wide else 13
	for node in nodes:
		props = write_props(node.props)
		start = offset + len(out)
		body_start = start + head + len(node.name) + len(props)
		children = b""
		if node.has_block:
			children = write_nodes(node.children, body_start, wide) + null_record(wide)
		end_offset = body_start + len(children)
		fmt = "<QQQ" if wide else "<III"
		out += struct.pack(fmt, end_offset, len(node.props), len(props)) + bytes([len(node.name)]) + node.name + props + children
	return bytes(out)


def null_record(wide):
	return b"\x00" * (25 if wide else 13)


def rename(nodes, old, new):
	count = 0
	for node in nodes:
		for i, (code, raw) in enumerate(node.props):
			if code == b"S" and raw.startswith(old + b"\x00\x01"):
				node.props[i] = (code, new + raw[len(old):])
				count += 1
			elif code == b"S" and raw == old:
				node.props[i] = (code, new)
				count += 1
		count += rename(node.children, old, new)
	return count


def convert(src, dst, old, new):
	data = open(src, "rb").read()
	if not data.startswith(MAGIC):
		raise ValueError(f"{src}: not a binary FBX")
	(version,) = struct.unpack_from("<I", data, 23)
	wide = version >= 7500
	nodes, pos = read_nodes(data, 27, len(data), wide)
	count = rename(nodes, old.encode(), new.encode())
	body = write_nodes(nodes, 27, wide) + null_record(wide)
	# the footer (padding, version, magic) after the top-level null record is kept as it is
	footer = data[pos:]
	os.makedirs(os.path.dirname(dst), exist_ok=True)
	with open(dst, "wb") as f:
		f.write(data[:27] + body + footer)
	return count


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument("src")
	ap.add_argument("dst")
	ap.add_argument("--old", default="Armature.001")
	ap.add_argument("--new", default="Armature")
	args = ap.parse_args()
	total = 0
	for root, _, files in os.walk(args.src):
		for name in files:
			src = os.path.join(root, name)
			dst = os.path.join(args.dst, os.path.relpath(src, args.src))
			if name.lower().endswith(".fbx"):
				n = convert(src, dst, args.old, args.new)
				total += 1
				print(f"{os.path.relpath(src, args.src)}: {n} name(s) renamed")
			else:
				os.makedirs(os.path.dirname(dst), exist_ok=True)
				shutil.copy2(src, dst)
	print(f"{total} FBX written to {args.dst}")


if __name__ == "__main__":
	main()
