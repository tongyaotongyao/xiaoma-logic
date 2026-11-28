#!/usr/bin/env python3
"""开发者工具：读取/改写 XiaomaLogic 进度存档（UE GVAS 二进制）。

用途：开发期直接解锁全部关卡，不用一关一关打过去。

    python Tools/unlock_save.py                # 打印当前存档并解锁全部 25 关
    python Tools/unlock_save.py --dry-run      # 只看不改
    python Tools/unlock_save.py --unlock 10    # 只解锁到第 10 关
    python Tools/unlock_save.py --stars 3      # 顺便把所有关卡标成三星
    python Tools/unlock_save.py --stars 0      # 清空星级（不影响解锁）

实现说明（UE 5.5 的 FPropertyTag 布局，见 Engine/.../PropertyTag.cpp）：
    名字 FString
    完整类型名 FPropertyTypeName：FString 类型名 + int32 参数个数 + 递归参数
    长度 int32
    标志 uint8          0x01 带 ArrayIndex / 0x02 带 PropertyGuid /
                        0x04 带扩展 / 0x10 布尔为真
    可选 ArrayIndex int32、PropertyGuid 16 字节
    值（长度字节）

只改目标值所在的那几个字节，其余部分原样保留；写盘前会自动备份，
写完会重新解析一遍做自检。
"""

from __future__ import annotations

import argparse
import shutil
import struct
import sys
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SAVE = PROJECT_ROOT / "Saved" / "SaveGames" / "XiaomaLogicProgress.sav"
LEVEL_COUNT = 25

FLAG_HAS_ARRAY_INDEX = 0x01
FLAG_HAS_PROPERTY_GUID = 0x02
FLAG_HAS_EXTENSIONS = 0x04
FLAG_BOOL_TRUE = 0x10


class SaveFormatError(Exception):
    pass


class Reader:
    def __init__(self, data: bytes, offset: int = 0):
        self.data = data
        self.offset = offset

    def u8(self) -> int:
        value = self.data[self.offset]
        self.offset += 1
        return value

    def i32(self) -> int:
        value = struct.unpack_from("<i", self.data, self.offset)[0]
        self.offset += 4
        return value

    def raw(self, count: int) -> bytes:
        if count < 0 or self.offset + count > len(self.data):
            raise SaveFormatError("存档被截断（偏移 %#x，需要 %d 字节）" % (self.offset, count))
        value = self.data[self.offset:self.offset + count]
        self.offset += count
        return value

    def fstring(self) -> str:
        length = self.i32()
        if length == 0:
            return ""
        if length < 0:
            return self.raw(-length * 2).decode("utf-16-le").rstrip("\x00")
        return self.raw(length).rstrip(b"\x00").decode("latin-1")

    def typename(self) -> tuple:
        """FPropertyTypeName：名字 + 参数个数 + 递归参数。"""
        name = self.fstring()
        count = self.i32()
        if not 0 <= count <= 8:
            raise SaveFormatError("类型名参数个数异常：%d（偏移 %#x）" % (count, self.offset))
        params = tuple(self.typename() for _ in range(count))
        return (name,) + params


def type_label(type_name) -> str:
    """把嵌套的 FPropertyTypeName 打成 ArrayProperty<IntProperty> 这种可读形式。"""
    if len(type_name) == 1:
        return type_name[0]
    return "%s<%s>" % (type_name[0], ", ".join(type_label(part) for part in type_name[1:]))


def is_int(prop) -> bool:
    return prop.type_name == ("IntProperty",)


def is_int_array(prop) -> bool:
    return prop.type_name == ("ArrayProperty", ("IntProperty",))


def is_bool(prop) -> bool:
    return prop.type_name == ("BoolProperty",)


def is_byte_array(prop) -> bool:
    return prop.type_name == ("ArrayProperty", ("ByteProperty",))


class Property:
    def __init__(self, name, type_name, size, flags, start, value_start, value_end):
        self.name = name
        self.type_name = type_name
        self.size = size
        self.flags = flags
        self.start = start
        self.value_start = value_start
        self.value_end = value_end


def parse_header(data: bytes) -> int:
    """跳过 GVAS 头部，返回 tagged property 流起始偏移。"""
    if data[:4] != b"GVAS":
        raise SaveFormatError("不是 UE 存盘文件（缺少 GVAS 标记）")
    r = Reader(data, 4)
    r.i32()                      # SaveGameFileVersion
    r.i32()                      # PackageFileUEVersion (UE4)
    r.i32()                      # PackageFileUEVersion (UE5)
    r.raw(6)                     # EngineVersion: Major / Minor / Patch
    r.i32()                      # Changelist
    r.fstring()                  # Branch
    r.i32()                      # CustomVersionFormat
    count = r.i32()              # 自定义版本条数
    if not 0 <= count <= 512:
        raise SaveFormatError("自定义版本条数异常：%d" % count)
    r.raw(count * 20)            # 每条 = 16 字节 GUID + 4 字节版本号
    r.fstring()                  # 存档对象的类名
    r.u8()                       # 类名后有一个恒为 0 的字节
    return r.offset


def parse_properties(data: bytes, offset: int):
    props = []
    r = Reader(data, offset)
    while True:
        start = r.offset
        name = r.fstring()
        if name == "None":
            break
        type_name = r.typename()
        size = r.i32()
        flags = r.u8()
        if flags & FLAG_HAS_ARRAY_INDEX:
            r.i32()
        if flags & FLAG_HAS_PROPERTY_GUID:
            r.raw(16)
        if flags & FLAG_HAS_EXTENSIONS:
            raise SaveFormatError("属性 %s 带扩展字段，本工具暂不支持" % name)
        value_start = r.offset
        r.raw(size)
        props.append(Property(name, type_name, size, flags, start, value_start, r.offset))
    return props, r.offset


def load(path: Path):
    data = path.read_bytes()
    offset = parse_header(data)
    props, _ = parse_properties(data, offset)
    return data, props


def int_value(data: bytes, prop: Property) -> int:
    return struct.unpack_from("<i", data, prop.value_start)[0]


def int_array(data: bytes, prop: Property):
    """返回 (count, [int...])。"""
    if not is_int_array(prop):
        raise SaveFormatError("%s 不是整数数组：%s" % (prop.name, type_label(prop.type_name)))
    count = struct.unpack_from("<i", data, prop.value_start)[0]
    if prop.size != 4 + count * 4:
        raise SaveFormatError("%s 长度不自洽（size=%d, count=%d）" % (prop.name, prop.size, count))
    values = list(struct.unpack_from("<%di" % count, data, prop.value_start + 4))
    return count, values


def show(data: bytes, props) -> None:
    for prop in props:
        if is_int(prop):
            print("  %-18s %d" % (prop.name, int_value(data, prop)))
        elif is_bool(prop):
            print("  %-18s %s" % (prop.name, bool(prop.flags & FLAG_BOOL_TRUE)))
        elif is_int_array(prop):
            count, values = int_array(data, prop)
            shown = values if count <= LEVEL_COUNT else values[:8] + ["..."]
            print("  %-18s %s（%d 项）" % (prop.name, shown, count))
        elif is_byte_array(prop):
            print("  %-18s %d 个字节" % (prop.name, prop.size - 4))
        else:
            print("  %-18s <%s> %d 字节" % (prop.name, type_label(prop.type_name), prop.size))


def patch(path: Path, unlock, stars) -> list:
    data, props = load(path)
    by_name = {prop.name: prop for prop in props}
    edits = []
    lines = []

    if unlock is not None:
        target = max(1, min(LEVEL_COUNT, unlock))
        prop = by_name.get("UnlockedLevel")
        if prop is None:
            raise SystemExit("存档里没有 UnlockedLevel 属性；先在游戏里玩一关再运行本工具。")
        old = int_value(data, prop)
        if old != target:
            edits.append((prop.value_start, prop.value_end, struct.pack("<i", target)))
            lines.append("UnlockedLevel：%d → %d" % (old, target))
        else:
            lines.append("UnlockedLevel 已是 %d，未改动" % target)

    if stars is not None:
        value = max(0, min(3, stars))
        prop = by_name.get("StarsByLevel")
        if prop is None:
            lines.append("存档里没有 StarsByLevel（全 0 时 UE 不写盘），跳过星级")
        else:
            count, values = int_array(data, prop)
            wanted = [value] * LEVEL_COUNT
            if count != LEVEL_COUNT:
                raise SystemExit(
                    "StarsByLevel 有 %d 项，期望 %d 项；为避免改动文件长度，本工具不处理这种情况。"
                    % (count, LEVEL_COUNT))
            if values != wanted:
                payload = struct.pack("<i", LEVEL_COUNT) + struct.pack("<%di" % LEVEL_COUNT, *wanted)
                edits.append((prop.value_start, prop.value_end, payload))
                lines.append("StarsByLevel：%s → %s" % (values, wanted))
            else:
                lines.append("StarsByLevel 已是全 %d 星，未改动" % value)

    if not edits:
        return lines

    out = bytearray()
    cursor = 0
    for start, end, payload in sorted(edits):
        out += data[cursor:start]
        out += payload
        cursor = end
    out += data[cursor:]

    # 自检：重新解析，确认结构与新值都对
    offset = parse_header(bytes(out))
    new_props, _ = parse_properties(bytes(out), offset)
    new_by_name = {prop.name: prop for prop in new_props}
    if unlock is not None:
        got = int_value(bytes(out), new_by_name["UnlockedLevel"])
        if got != max(1, min(LEVEL_COUNT, unlock)):
            raise SystemExit("自检失败：UnlockedLevel 写成 %d" % got)
    if stars is not None and "StarsByLevel" in new_by_name:
        count, values = int_array(bytes(out), new_by_name["StarsByLevel"])
        if count != LEVEL_COUNT or set(values) != {max(0, min(3, stars))}:
            raise SystemExit("自检失败：StarsByLevel 写坏")

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = path.with_name(path.name + ".bak-" + stamp)
    shutil.copy2(path, backup)
    path.write_bytes(bytes(out))
    lines.append("已写入存档（%d 字节，长度未变）" % len(out))
    lines.append("备份：%s" % backup.name)
    return lines


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="查看/改写 XiaomaLogic 存档（解锁关卡、设置星级）")
    parser.add_argument("--path", type=Path, default=DEFAULT_SAVE, help="存档路径")
    parser.add_argument("--unlock", type=int, default=LEVEL_COUNT,
                        help="解锁到第几关，默认 %d；0 表示不改" % LEVEL_COUNT)
    parser.add_argument("--stars", type=int, default=None, help="把所有关卡星级设为 n（0~3），默认不改")
    parser.add_argument("--dry-run", action="store_true", help="只打印，不写盘")
    args = parser.parse_args(argv)

    path = args.path
    if not path.exists():
        print("找不到存档：%s" % path, file=sys.stderr)
        print("先启动一次游戏（进主菜单即可）让它生成存档。", file=sys.stderr)
        return 1

    data, props = load(path)
    print("存档：%s" % path)
    show(data, props)

    if args.dry_run:
        return 0

    unlock = None if args.unlock == 0 else args.unlock
    print()
    for line in patch(path, unlock, args.stars):
        print("  " + line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
