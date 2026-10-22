"""Full cvar registry: name, type, registration default, flags, allowModify (const), help, backing var.
usage: run inside IDA 9 (GUI or an idalib runner) with an analysed WHGame.dll database open, and
sys.argv = [script, in.jsonl, out.jsonl]. in.jsonl lists already-known registrations in the output format (only "name", "func"
and "help" are read; ../data/cvars-WHGame-1.9.8.jsonl or an empty file both work).
The output for Steam 1.9.8 is ../data/cvars-WHGame-1.9.8.jsonl. The gEnv->pConsole RVA below is for that build.

Walks every function that touches gEnv->pConsole (qword_1829D1768) or already registers a cvar per cvars.jsonl, and
tracks constant register/stack values linearly through each chunk. At every `call [rax+slot]` whose rdx is a string it
decodes the IConsole::Register* overload by vtable slot (see ../cfg-loading.md):
  0x10 RegisterString(name, def, flags, help, cb)      0x38 Register(name, const char** var, def, flags, help, cb, allowModify)
  0x18 RegisterInt(name, def, flags, help, cb)         0x40 Register(name, int* var, def, flags, help, cb, allowModify)
  0x28 RegisterFloat(name, def, flags, help, cb)       0x48 Register(name, float* var, def, flags, help, cb, allowModify)
allowModify=false makes the engine OR in VF_CONST_CVAR (0x800000), which retail cfg loading refuses without -devmode.
Linear tracking ignores branches; values it can't resolve come out as None or '?<mnemonic>'."""
import json
import re
import struct
import sys

import ida_bytes
import ida_funcs
import ida_idp
import ida_nalt
import ida_ua
import idaapi
import idautils
import idc

base = idaapi.get_imagebase()
CONSOLE = base + 0x29D1768  # gEnv->pConsole
NAMERX = re.compile(r"^[A-Za-z][A-Za-z0-9_.]{2,}$")
VOLATILE = {"rax", "rcx", "rdx", "r8", "r9", "r10", "r11"} | {f"xmm{i}" for i in range(6)}
SUBREG = {"eax": "rax", "ebx": "rbx", "ecx": "rcx", "edx": "rdx", "esi": "rsi", "edi": "rdi", "ebp": "rbp",
          "esp": "rsp", "al": "rax", "bl": "rbx", "cl": "rcx", "dl": "rdx", "sil": "rsi", "dil": "rdi", "bpl": "rbp"}
# slot -> (type, default, flags, help, allowModify) argument locations
SLOTS = {0x10: ("string", "r8", "r9", "rsp+20", None), 0x18: ("int", "r8", "r9", "rsp+20", None),
         0x28: ("float", "xmm2", "r9", "rsp+20", None), 0x38: ("string", "r9", "rsp+20", "rsp+28", "rsp+38"),
         0x40: ("int", "r9", "rsp+20", "rsp+28", "rsp+38"), 0x48: ("float", "xmm3", "rsp+20", "rsp+28", "rsp+38")}


def norm(r):
    r = r.lower()
    if r in SUBREG:
        return SUBREG[r]
    if r.startswith("r") and r[-1] in "dwb" and r[1:-1].isdigit():
        return r[:-1]
    return r


def string_at(ea):
    if ida_bytes.is_strlit(ida_bytes.get_flags(ea)):
        v = idc.get_strlit_contents(ea, -1, ida_nalt.STRTYPE_C)
        return v.decode("latin-1") if v else None
    s = (ida_bytes.get_bytes(ea, 128) or b"").split(b"\0")[0]
    return s.decode() if s and all(32 <= c < 127 for c in s) else None


def src_value(insn, st):
    op = insn.ops[1]
    if op.type == ida_ua.o_imm:
        return ("imm", op.value & 0xFFFFFFFFFFFFFFFF)
    if op.type == ida_ua.o_reg:
        return st.get(norm(idc.print_operand(insn.ea, 1)), ("?", idc.print_operand(insn.ea, 1)))
    if op.type == ida_ua.o_mem:
        return ("mem", op.addr)
    return ("?", idc.print_operand(insn.ea, 1))


def dst_key(insn):
    op = insn.ops[0]
    if op.type == ida_ua.o_reg:
        return norm(idc.print_operand(insn.ea, 0))
    if op.type == ida_ua.o_displ and op.phrase == 4:  # [rsp+X]: outgoing stack argument
        return f"rsp+{op.addr:x}"
    return None


def step(insn, st):
    k = dst_key(insn)
    if not k:
        return
    mn = insn.get_canon_mnem()
    o1 = insn.ops[1]
    if mn in ("mov", "movzx", "movsxd"):
        st[k] = src_value(insn, st)
    elif mn == "lea":
        if o1.type == ida_ua.o_mem:
            st[k] = ("mem", o1.addr)
        elif o1.type in (ida_ua.o_displ, ida_ua.o_phrase) and o1.specflag1 == 0:  # lea r12d, [r10+1]
            bv = st.get(norm(ida_idp.get_reg_name(o1.phrase, 8)))
            disp = o1.addr if o1.type == ida_ua.o_displ else 0
            st[k] = ("imm", bv[1] + disp) if bv and bv[0] == "imm" else ("?", "lea")
        else:
            st[k] = ("?", "lea")
    elif mn in ("xor", "xorps", "pxor") and o1.type == ida_ua.o_reg and \
            idc.print_operand(insn.ea, 0) == idc.print_operand(insn.ea, 1):
        st[k] = ("imm", 0)
    elif mn in ("movss", "movaps", "movd", "movsd", "cvtsi2ss"):
        v = src_value(insn, st)
        if v[0] == "mem" and mn in ("movss", "movd"):
            v = ("flt", struct.unpack("<f", ida_bytes.get_bytes(v[1], 4))[0])
        elif v[0] == "imm" and mn == "movd":
            v = ("flt", struct.unpack("<f", struct.pack("<I", v[1] & 0xFFFFFFFF))[0])
        st[k] = v
    elif mn == "or" and k in st and st[k][0] == "imm" and o1.type == ida_ua.o_imm:
        st[k] = ("imm", st[k][1] | o1.value)
    else:
        st[k] = ("?", mn)


def fmt(v, kind):
    if v is None:
        return None
    t, x = v
    if t == "imm":
        if kind == "float":
            return struct.unpack("<f", struct.pack("<I", x & 0xFFFFFFFF))[0] if x else 0.0
        if kind == "int" and x & 0x80000000 and x < (1 << 32):
            return x - (1 << 32)
        return x
    if t == "flt":
        return x
    if t == "mem":
        return string_at(x) if kind == "string" else f"mem:{x - base:#x}"
    return f"?{x}"


def registrations(start, end, trusted):
    st, ea, insn = {}, start, ida_ua.insn_t()
    while ea < end and ea != idc.BADADDR:
        if ida_ua.decode_insn(insn, ea):
            if insn.get_canon_mnem() == "call":
                op = insn.ops[0]
                rd, rc = st.get("rdx"), st.get("rcx")
                if op.type == ida_ua.o_displ and op.addr in SLOTS and rd and rd[0] == "mem" and \
                        (trusted or rc == ("mem", CONSOLE)):
                    name = string_at(rd[1])
                    if name and NAMERX.match(name):
                        yield ea, name, dict(st)
                for v in VOLATILE:
                    st.pop(v, None)
            else:
                step(insn, st)
        ea = idc.next_head(ea, end)


rows = [json.loads(line) for line in open(sys.argv[1])]
known_funcs = {base + int(r["func"], 16) for r in rows}
known_help = {r["name"]: r["help"] for r in rows if r["help"]}
funcs = set(known_funcs) | {f.start_ea for x in idautils.XrefsTo(CONSOLE) if (f := ida_funcs.get_func(x.frm))}
n, seen = 0, set()
with open(sys.argv[2], "w") as out:
    for fs in sorted(funcs):
        if not ida_funcs.get_func(fs):
            continue
        for ch_start, ch_end in idautils.Chunks(fs):
            for call, name, st in registrations(ch_start, ch_end, fs in known_funcs):
                if call in seen:
                    continue
                seen.add(call)
                slot = idc.get_operand_value(call, 0)
                typ, d_loc, f_loc, h_loc, am_loc = SLOTS[slot]
                flags = st.get(f_loc)
                am = st.get(am_loc) if am_loc else ("imm", 1)
                var = st.get("r8") if slot >= 0x38 else None
                out.write(json.dumps(dict(
                    name=name, type=typ, by_ref=slot >= 0x38, default=fmt(st.get(d_loc), typ),
                    flags=flags[1] if flags and flags[0] == "imm" else None,
                    allow_modify=(am[1] != 0) if am and am[0] == "imm" else None,
                    help=fmt(st.get(h_loc), "string") or known_help.get(name),
                    var=hex(var[1] - base) if var and var[0] == "mem" else None,
                    func=hex(fs - base), call=hex(call - base))) + "\n")
                n += 1
print("wrote", n)
