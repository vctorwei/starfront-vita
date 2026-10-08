#!/usr/bin/env python3
"""Validate static Vita build and VPK evidence, not runtime playability.

Run with .venv/bin/python scripts/validate_build.py.  The only optional write
is the JSON report requested with --output; build artifacts are never changed.
"""

import argparse
import hashlib
import io
import json
import re
import struct
import sys
import zipfile
import zlib
from collections import Counter
from pathlib import Path

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    sys.exit("pyelftools is required; install it with python3 -m pip install pyelftools")


ROOT = Path(__file__).resolve().parents[1]
STACK_SIZE = 2 * 1024 * 1024
EXPECTED_IMPORTS = 351
GL_TRACE_TARGETS = {n: 'sf_trace_' + n for n in [
    "glGetError", "glClearColor", "glClear", "glViewport", "glBindFramebuffer",
    "glUseProgram", "glDrawArrays", "glDrawElements", "glCompressedTexImage2D", "glTexImage2D",
    "glPixelStorei", "glActiveTexture", "glBindTexture", "glGetUniformLocation", "glGetAttribLocation",
    "glUniform1i", "glUniform1iv", "glUniformMatrix4fv", "glVertexAttribPointer",
    "glBindBuffer", "glEnable", "glDisable", "glColorMask", "glScissor", "glCullFace", "glFrontFace",
    "glEnableVertexAttribArray", "glDisableVertexAttribArray",
]}
GL_TRACE_TARGETS.update({'sf_'+n:'sf_trace_'+n for n in ['glViewport','glScissor','glBindFramebuffer']})
BRIDGES = {
    "close": "sf_close",
    "fcntl": "sf_fcntl",
    "fstat": "sf_fstat",
    "gethostbyname": "sf_gethostbyname",
    "gethostname": "sf_gethostname",
    "glCompileShader": "sf_glCompileShader",
    "glLinkProgram": "sf_glLinkProgram",
    "glBlendColor": "sf_glBlendColor",
    "glBlendFunc": "sf_glBlendFunc",
    "glGetFloatv": "sf_glGetFloatv",
    "glViewport": "sf_glViewport",
    "glScissor": "sf_glScissor",
    "glBindFramebuffer": "sf_glBindFramebuffer",
    "glBindFramebufferOES": "sf_glBindFramebuffer",
    "glGetIntegerv": "sf_glGetIntegerv",
    "pthread_create": "sf_pthread_create",
    "sigaction": "sf_sigaction",
    "socket": "sf_socket",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def resolve(path):
    path = Path(path)
    return (path if path.is_absolute() else ROOT / path).resolve()


def bounded(data, offset, size, description):
    require(offset >= 0 and size >= 0 and offset + size <= len(data),
            f"{description} is outside the file")
    return data[offset:offset + size]


class ElfImage:
    def __init__(self, path):
        self.path = path
        self.data = path.read_bytes()
        self.elf = ELFFile(io.BytesIO(self.data))
        self.symbols = {}
        self.undefined = []
        for section in self.elf.iter_sections():
            if section["sh_type"] not in ("SHT_SYMTAB", "SHT_DYNSYM"):
                continue
            for symbol in section.iter_symbols():
                if not symbol.name:
                    continue
                if symbol["st_shndx"] == "SHN_UNDEF":
                    self.undefined.append(symbol)
                else:
                    self.symbols[symbol.name] = symbol

    def read(self, address, length):
        for segment in self.elf.iter_segments():
            if segment["p_type"] != "PT_LOAD":
                continue
            start = segment["p_vaddr"]
            if start <= address and address + length <= start + segment["p_filesz"]:
                offset = address - start
                return segment.data()[offset:offset + length]
        raise ValueError(f"0x{address:08x} ({length} bytes) is not file-backed PT_LOAD data")

    def uint32(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]

    def cstring(self, address):
        for segment in self.elf.iter_segments():
            start = segment["p_vaddr"]
            if (segment["p_type"] == "PT_LOAD" and
                    start <= address < start + segment["p_filesz"]):
                data = segment.data()[address - start:]
                end = data.find(b"\0")
                require(end >= 0, f"unterminated string at 0x{address:08x}")
                return data[:end].decode("utf-8")
        raise ValueError(f"string address 0x{address:08x} is not in PT_LOAD data")

    def symbol(self, name):
        require(name in self.symbols, f"defined ELF symbol missing: {name}")
        return self.symbols[name]

    def raw_program_headers(self):
        elf = self.elf
        require(elf.elfclass == 32 and elf.little_endian, "expected ELF32 little endian")
        require(elf["e_phentsize"] == 32, "unexpected ELF32 program header size")
        return [struct.unpack("<8I", bounded(self.data, elf["e_phoff"] + i * 32,
                                            32, "ELF program header"))
                for i in range(elf["e_phnum"])]


def parse_sfo(data):
    magic, version, keys_at, values_at, count = struct.unpack(
        "<5I", bounded(data, 0, 20, "SFO header"))
    require(magic == 0x46535000 and version == 0x101, "invalid SFO magic/version")
    require(20 + 16 * count <= keys_at <= values_at <= len(data), "invalid SFO tables")
    result = {}
    for i in range(count):
        key_offset, format_id, length, maximum, value_offset = struct.unpack(
            "<HHIII", bounded(data, 20 + 16 * i, 16, "SFO entry"))
        start = keys_at + key_offset
        require(keys_at <= start < values_at, "SFO key offset outside key table")
        end = data.find(b"\0", start, values_at)
        require(end >= 0, "unterminated SFO key")
        key = data[start:end].decode("utf-8")
        require(key not in result, f"duplicate SFO key: {key}")
        require(length <= maximum, f"invalid SFO length for {key}")
        value_at = values_at + value_offset
        bounded(data, value_at, maximum, f"SFO allocated value for {key}")
        value = bounded(data, value_at, length, f"SFO value for {key}")
        if format_id == 0x204:
            require(value.endswith(b"\0"), f"SFO string lacks terminator: {key}")
            value = value[:-1].decode("utf-8")
        elif format_id == 0x404:
            require(length == 4, f"invalid SFO integer length: {key}")
            value = struct.unpack("<I", value)[0]
        else:
            raise ValueError(f"unsupported SFO format 0x{format_id:x} for {key}")
        result[key] = value
    return result


def unpack_self(data, velf):
    require(bounded(data, 0, 4, "SELF magic") == b"SCE\0", "eboot lacks SCE magic")
    version = struct.unpack_from("<I", bounded(data, 0, 0x60, "SELF header"), 4)[0]
    require(version == 3, f"unexpected SELF version: {version}")
    file_size = struct.unpack_from("<Q", data, 0x20)[0]
    require(file_size == len(data), "SELF declared file size differs from actual bytes")
    elf_at, ph_at, _, segment_at = struct.unpack_from("<4Q", data, 0x40)
    header = bounded(data, elf_at, 52, "embedded ELF header")
    require(header[:7] == b"\x7fELF\x01\x01\x01", "SELF embedded ELF is not ELF32 LE")
    require(struct.unpack_from("<H", header, 18)[0] == 40, "SELF embedded ELF is not ARM")
    ph_size, ph_count = struct.unpack_from("<HH", header, 42)
    require(ph_size == 32, "unexpected SELF program header size")
    reference = velf.raw_program_headers()
    require(ph_count == len(reference), "SELF/velf program header counts differ")
    # vita-make-fself writes a NID derived from SHA256(SHA256(input.velf))
    # into sce_module_info_raw.module_nid (+52), and caps p_align at 0x1000.
    # https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself/vita-make-fself.c
    # https://github.com/vitasdk/vita-toolchain/blob/master/src/utils/sha256.c
    require(struct.unpack_from("<H", header, 16)[0] == 0xFE04,
            "validator expects ET_SCE_RELEXEC from vita-elf-create")
    entry = velf.elf["e_entry"]
    require(struct.unpack_from("<I", header, 24)[0] == entry,
            "SELF/velf module entry pointers differ")
    module_segment, module_offset = entry >> 30, entry & 0x3FFFFFFF
    require(module_segment < len(reference), "module entry segment index is invalid")
    module_section = velf.elf.get_section_by_name(".sceModuleInfo.rodata")
    require(module_section is not None and
            module_section["sh_addr"] == reference[module_segment][2] + module_offset,
            "module entry does not identify .sceModuleInfo.rodata")
    nid = int.from_bytes(hashlib.sha256(hashlib.sha256(velf.data).digest()).digest()[:4], "big")
    segments = []
    for i in range(ph_count):
        ph = struct.unpack("<8I", bounded(data, ph_at + i * 32, 32, "SELF program header"))
        offset, length, compression, encryption = struct.unpack(
            "<4Q", bounded(data, segment_at + i * 32, 32, "SELF segment info"))
        require(encryption == 2, f"SELF segment {i} is not plaintext")
        raw = bounded(data, offset, length, f"SELF segment {i}")
        if compression == 2:
            decoder = zlib.decompressobj()
            decoded = decoder.decompress(raw) + decoder.flush()
            require(decoder.eof and len(decoder.unused_data) <= 3 and
                    not any(decoder.unused_data) and not decoder.unconsumed_tail,
                    f"invalid compressed SELF segment {i}")
        elif compression == 1:
            decoded = raw[:ph[4]]
            require(len(raw) - ph[4] <= 3 and not any(raw[ph[4]:]),
                    f"invalid uncompressed SELF padding in segment {i}")
        else:
            raise ValueError(f"unsupported compression for SELF segment {i}: {compression}")
        require(len(decoded) == ph[4], f"SELF segment {i} decoded size differs from p_filesz")
        expected_header = (*reference[i][:-1], min(reference[i][-1], 0x1000))
        require(ph == expected_header,
                f"SELF/velf segment {i} headers differ")
        expected = bytearray(bounded(velf.data, reference[i][1], reference[i][4], f"velf segment {i}"))
        if i == module_segment:
            bounded(expected, module_offset + 52, 4, "module NID field")
            struct.pack_into("<I", expected, module_offset + 52, nid)
        require(decoded == expected, f"SELF/velf segment {i} payloads differ")
        segments.append({"type": ph[0], "address": ph[2], "data": decoded,
                         "filesz": ph[4], "sha256": sha256(decoded), "alignment": ph[7],
                         "module_nid": f"0x{nid:08x}" if i == module_segment else None})
    return segments


class Validator:
    def __init__(self, build_dir, vpk):
        self.build_dir, self.vpk = build_dir, vpk
        self.checks, self.artifacts, self.counts = [], {}, {}
        self.images = {}
        self.members = None
        self.self_segments = None

    def check(self, name, function):
        try:
            detail = function()
            self.checks.append({"name": name, "passed": True, "details": detail})
        except Exception as exc:
            self.checks.append({"name": name, "passed": False,
                                "error": f"{type(exc).__name__}: {exc}"})

    def image(self, name):
        if name not in self.images:
            self.images[name] = ElfImage(self.build_dir / name)
        return self.images[name]

    def package(self):
        if self.members is None:
            with zipfile.ZipFile(self.vpk) as archive:
                names = archive.namelist()
                require(len(set(names)) == len(names), "VPK contains duplicate member names")
                bad = archive.testzip()
                require(bad is None, f"VPK CRC failure: {bad}")
                self.members = {name: archive.read(name) for name in names}
        return self.members

    def inputs(self):
        paths = {"elf": self.build_dir / "starfront", "velf": self.build_dir / "starfront.velf",
                 "eboot": self.build_dir / "eboot.bin", "vpk": self.vpk}
        missing = []
        for key, path in paths.items():
            if not path.is_file():
                missing.append(str(path))
                continue
            data = path.read_bytes()
            self.artifacts[key] = {"path": str(path), "size": len(data), "sha256": sha256(data)}
        require(not missing, "required artifacts missing: " + ", ".join(missing))
        return {"files": list(paths)}

    def arm_abi(self, name):
        elf = self.image(name).elf
        flags = elf["e_flags"]
        require(elf.elfclass == 32 and elf.little_endian and elf["e_machine"] == "EM_ARM",
                f"{name} is not ARM32 little endian")
        require((flags >> 24) == 5, f"{name} is not EABI version 5")
        require(flags & 0x200 and not flags & 0x400, f"{name} lacks soft-float ABI or declares hard-float ABI")
        vfp_args = []
        section = elf.get_section_by_name(".ARM.attributes")
        if section:
            for subsection in section.iter_subsections():
                for subsubsection in subsection.iter_subsubsections():
                    for attribute in subsubsection.iter_attributes():
                        if attribute.tag == "TAG_ABI_VFP_ARGS":
                            vfp_args.append(attribute.value)
        require(all(value == 0 for value in vfp_args), f"{name} uses VFP register argument ABI")
        return {"machine": elf["e_machine"], "bits": elf.elfclass, "little_endian": elf.little_endian,
                "flags": f"0x{flags:08x}", "eabi": flags >> 24,
                "float_abi": "soft/softfp (base AAPCS)", "vfp_argument_attributes": vfp_args}

    def undefined_symbols(self, name):
        symbols = self.image(name).undefined
        strong = sorted({s.name for s in symbols if s["st_info"]["bind"] != "STB_WEAK"})
        require(not strong, f"{name} has strong undefined symbols: {', '.join(strong)}")
        return {"strong_undefined": strong, "weak_undefined": sorted({s.name for s in symbols})}

    def zip_integrity(self):
        members = self.package()
        required = {"eboot.bin", "sce_sys/param.sfo", "COPYING", "DISTRIBUTION.txt"}
        require(required <= members.keys(), "VPK lacks members: " + ", ".join(sorted(required - members.keys())))
        return {"crc": "all members passed", "members": sorted(members)}

    def packaged_eboot(self):
        packaged = self.package()["eboot.bin"]
        require(packaged[:4] == b"SCE\0", "packaged eboot lacks SCE magic")
        require(packaged == (self.build_dir / "eboot.bin").read_bytes(), "VPK eboot differs from build eboot")
        self.self_segments = unpack_self(packaged, self.image("starfront.velf"))
        return {"magic": "SCE\\0", "size": len(packaged), "sha256": sha256(packaged),
                "byte_exact_build_match": True,
                "velf_segments": [{k: v for k, v in segment.items() if k != "data"}
                                  for segment in self.self_segments]}

    def metadata(self):
        cache = (self.build_dir / "CMakeCache.txt").read_text()
        values = re.findall(r"^STARFRONT_AUTOSTART:BOOL=(.*)$", cache, re.MULTILINE)
        require(len(values) == 1, "CMakeCache lacks unique STARFRONT_AUTOSTART BOOL")
        token = values[0].strip().upper()
        require(token in {"ON", "OFF", "1", "0", "YES", "NO", "TRUE", "FALSE", "Y", "N"},
                f"unrecognized STARFRONT_AUTOSTART value: {token}")
        autostart = token in {"ON", "1", "YES", "TRUE", "Y"}
        expected_title = "Starfront Emulator Test" if autostart else "Starfront Test"
        sfo = parse_sfo(self.package()["sce_sys/param.sfo"])
        require(sfo.get("TITLE_ID") == "SFHP00001", "wrong SFO TITLE_ID")
        require(sfo.get("APP_VER") == "00.06", "wrong SFO APP_VER (expected 00.06)")
        require(sfo.get("TITLE") == expected_title and sfo.get("STITLE") == expected_title,
                f"SFO TITLE/STITLE disagree with AUTOSTART={token}")
        return {"autostart": autostart, "TITLE_ID": sfo["TITLE_ID"], "APP_VER": sfo["APP_VER"],
                "TITLE": sfo["TITLE"], "STITLE": sfo["STITLE"]}

    def distribution(self):
        allowed = {"eboot.bin", "sce_sys/param.sfo", "COPYING", "LICENSE.md", "THIRD_PARTY.md", "DISTRIBUTION.txt", "docs/BUILD.md"}
        expected = {}
        for path in (ROOT / "assets/sce_sys").rglob("*"):
            if path.is_file() and path.suffix in {".png", ".xml"}:
                name = "sce_sys/" + path.relative_to(ROOT / "assets/sce_sys").as_posix()
                expected[name] = path.read_bytes()
        for path in (ROOT / "licenses").glob("*.txt"):
            expected["licenses/" + path.name] = path.read_bytes()
        allowed.update(expected)
        require(set(self.package()) == allowed, "VPK member allowlist mismatch")
        for name, data in expected.items():
            require(self.package()[name] == data, "packaged source asset differs: " + name)
        elf = self.image("starfront").data
        require(b"Preflight passed. CROSS:" not in elf, "startup still asks for CROSS")
        require(b"Preflight passed. Starting game." in elf, "PSV automatic startup message absent")
        require(b"SCE CONFIDENTIAL" not in elf, "proprietary compiler text embedded")
        require(b"ux0:data/starfront/apk/igli.bin" in elf, "APK configuration path not relocated")
        require(b"ux0:data/starfront/apk/serialkey.txt" in elf, "APK serial path not relocated")
        cache = (self.build_dir / "CMakeCache.txt").read_text()
        require("STARFRONT_FILE_LOG:BOOL=OFF" in cache, "PSV file logging enabled")
        require("STARFRONT_NATIVE_HEIGHT:STRING=600" in cache, "release layout changed")
        return {"packaged_game_resources": False, "source_assets_match": True, "logical_height": 600, "file_logging": False}

    def imports(self):
        cache = (self.build_dir / "CMakeCache.txt").read_text()
        values = re.findall(r"^STARFRONT_GL_TRACE:BOOL=(.*)$", cache, re.MULTILINE)
        require(len(values) <= 1, "CMakeCache has duplicate STARFRONT_GL_TRACE BOOL")
        # Older normal builds predate the trace option and therefore have it off.
        token = values[0].strip().upper() if values else "OFF"
        require(token in {"ON", "OFF", "1", "0", "YES", "NO", "TRUE", "FALSE", "Y", "N"},
                f"unrecognized STARFRONT_GL_TRACE value: {token}")
        trace_enabled = token in {"ON", "1", "YES", "TRUE", "Y"}
        coverage = json.loads((ROOT / "tests/fixtures/import-coverage.json").read_text())
        native = [entry["import"] for entry in coverage]
        names = [entry["import"] for entry in coverage]
        statuses = Counter(entry["status"] for entry in coverage)
        mapped = sum(entry["status"] != "diagnostic trap" for entry in coverage)
        traps = statuses["diagnostic trap"]
        self.counts.update({"native_imports": len(native), "reported_imports": len(coverage),
                            "mapped": mapped, "diagnostic_traps": traps,
                            "by_status": dict(sorted(statuses.items()))})
        require(len(native) == EXPECTED_IMPORTS and len(set(native)) == len(native), "unexpected original import inventory")
        require(names == native and len(set(names)) == len(names), "coverage report does not exactly cover ordered native imports")
        require(set(statuses) <= {"SDK", "vitaGL", "bridge", "vitaGL alias", "diagnostic trap"}, "unknown import status")
        require(mapped + traps == len(coverage) == EXPECTED_IMPORTS,
                f"calculated import counts do not cover {EXPECTED_IMPORTS} imports: {mapped} mapped, {traps} traps")
        elf = self.image("starfront")
        table = elf.symbol("port_imports")
        declared_size = elf.uint32(elf.symbol("port_imports_size")["st_value"])
        require(table["st_size"] == declared_size == len(names) * 8, "actual import table size differs from report")
        mismatches = []
        key_addresses = {}
        trace_addresses = {}
        for i, entry in enumerate(coverage):
            name_ptr, address = struct.unpack("<II", elf.read(table["st_value"] + i * 8, 8))
            actual_name = elf.cstring(name_ptr)
            if entry["status"] == "diagnostic trap":
                require(entry["target"] is None, f"trap unexpectedly has a target: {entry['import']}")
                require(entry.get("trace_target") is None, f"trap unexpectedly has a trace target: {entry['import']}")
                target = f"import_{i}"
            else:
                target = entry["target"]
                require(isinstance(target, str) and target, f"mapped import lacks target: {entry['import']}")
                expected_trace = GL_TRACE_TARGETS.get(target)
                if "trace_target" in entry or trace_enabled:
                    require(entry.get("trace_target") == expected_trace,
                            f"incorrect trace target for {entry['import']}")
                if trace_enabled and expected_trace is not None:
                    target = expected_trace
            expected_address = elf.symbol(target)["st_value"]
            if actual_name != entry["import"] or address != expected_address:
                mismatches.append({"index": i, "reported_import": entry["import"], "actual_import": actual_name,
                                   "target": target, "actual_address": f"0x{address:08x}",
                                   "symbol_address": f"0x{expected_address:08x}"})
            if trace_enabled and entry.get("trace_target") is not None:
                trace_addresses[entry["import"]] = {"symbol": target, "table_address": f"0x{address:08x}",
                                                    "symbol_address": f"0x{expected_address:08x}"}
            if entry["import"] in BRIDGES:
                require(entry["status"] == "bridge" and entry["target"] == BRIDGES[entry["import"]],
                        f"critical import has incorrect bridge: {entry['import']}")
                key_addresses[entry["import"]] = {"symbol": target, "table_address": f"0x{address:08x}",
                                                   "symbol_address": f"0x{expected_address:08x}"}
        require(not mismatches, "actual import pointer/name mismatches: " + json.dumps(mismatches))
        require(set(key_addresses) == set(BRIDGES), "critical bridges absent from actual import table")
        return {"table_symbol_address": f"0x{table['st_value']:08x}", "table_size_bytes": declared_size,
                "all_entries_match_defined_symbols": True, "counts": dict(self.counts), "critical_bridges": key_addresses,
                "graphics_trace_enabled": trace_enabled, "trace_import_addresses": trace_addresses}

    def stack(self):
        velf = self.image("starfront.velf")
        section = velf.elf.get_section_by_name(".sceModuleInfo.rodata")
        require(section is not None, "velf lacks .sceModuleInfo.rodata")
        data = section.data()
        candidates = []
        for offset in range(4, len(data) - 3, 4):
            if data[offset:offset + 4] == b"PSP2" and struct.unpack_from("<I", data, offset - 4)[0] == 52:
                candidates.append(offset - 4)
        require(len(candidates) == 1, f"expected one 52-byte PSP2 process parameter, found {len(candidates)}")
        offset = candidates[0]
        parameter = bounded(data, offset, 52, "SCE process parameter")
        words = struct.unpack("<13I", parameter)
        require(words[2] == 6, f"unexpected SCE process parameter version: {words[2]}")
        pointer = words[6]  # sceUserMainThreadStackSize pointer at byte offset +24.
        require(pointer != 0, "SCE process parameter lacks explicit stack size pointer")
        actual = velf.uint32(pointer)
        regular = self.image("starfront")
        symbol = regular.symbol("sceUserMainThreadStackSize")
        require(symbol["st_size"] == 4, "unexpected stack size symbol size")
        require(pointer == symbol["st_value"], "process parameter stack pointer differs from regular ELF symbol")
        source_value = regular.uint32(symbol["st_value"])
        require(actual == source_value == STACK_SIZE, f"stack is not 2 MiB: velf={actual}, ELF={source_value}")
        if self.self_segments is None:
            self.self_segments = unpack_self(self.package()["eboot.bin"], velf)
        packed_value = None
        for segment in self.self_segments:
            if segment["type"] == 1 and segment["address"] <= pointer and pointer + 4 <= segment["address"] + segment["filesz"]:
                packed_value = struct.unpack_from("<I", segment["data"], pointer - segment["address"])[0]
        require(packed_value == STACK_SIZE, f"actual packed SELF stack value is not 2 MiB: {packed_value}")
        parameter_address = section["sh_addr"] + offset
        packed_parameter = None
        for segment in self.self_segments:
            if (segment["type"] == 1 and segment["address"] <= parameter_address and
                    parameter_address + 52 <= segment["address"] + segment["filesz"]):
                start = parameter_address - segment["address"]
                packed_parameter = segment["data"][start:start + 52]
        require(packed_parameter == parameter, "process parameter is not preserved in actual packed SELF")
        return {"process_parameter_address": f"0x{parameter_address:08x}", "process_parameter_size": 52,
                "process_parameter_version": words[2], "stack_pointer_offset": 24,
                "stack_size_address": f"0x{pointer:08x}", "elf_symbol_address": f"0x{symbol['st_value']:08x}",
                "regular_elf_stack_bytes": source_value, "velf_stack_bytes": actual,
                "packaged_self_stack_bytes": packed_value, "actual_process_parameter_packaged": True}

    def run(self):
        self.check("required_artifacts_and_hashes", self.inputs)
        for name in ("starfront", "starfront.velf"):
            self.check(f"{name}_arm32_eabi_softfloat", lambda name=name: self.arm_abi(name))
            self.check(f"{name}_no_strong_undefined", lambda name=name: self.undefined_symbols(name))
        self.check("vpk_zip_integrity", self.zip_integrity)
        self.check("packaged_eboot_matches_build_and_velf", self.packaged_eboot)
        self.check("sfo_matches_build_mode", self.metadata)
        self.check("distribution_resources_and_release_settings", self.distribution)
        self.check("import_coverage_and_actual_addresses", self.imports)
        self.check("actual_packaged_sce_stack_2mib", self.stack)
        failures = [check["name"] for check in self.checks if not check["passed"]]
        return {"success": not failures, "scope": "Static build/package checks only; runtime playability is not tested.",
                "build_dir": str(self.build_dir), "vpk": str(self.vpk), "artifacts": self.artifacts,
                "counts": self.counts, "checks": self.checks, "failures": failures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build", help="build directory (relative paths use project root)")
    parser.add_argument("--vpk", default="build/Starfront-test.vpk", help="VPK path (relative paths use project root)")
    parser.add_argument("--output", help="optional JSON report path (relative paths use project root)")
    args = parser.parse_args()
    report = Validator(resolve(args.build_dir), resolve(args.vpk)).run()
    serialized = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        output = resolve(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(serialized)
    sys.stdout.write(serialized)
    return 0 if report["success"] else 1


if __name__ == "__main__":
    sys.exit(main())
