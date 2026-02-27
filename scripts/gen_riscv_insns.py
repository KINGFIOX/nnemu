#!/usr/bin/env python3
"""
Generate RISC-V instruction sources for Meson build.
Reads instruction list from riscv_insn_list.txt and opcodes from encoding.h.
"""
import re
import sys
from pathlib import Path


def load_insn_list(list_path: Path) -> list[str]:
    """Load instruction list from riscv_insn_list.txt (one instruction per line)."""
    if not list_path.exists():
        raise FileNotFoundError(f"Instruction list not found: {list_path}")
    result = []
    for line in list_path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith('#'):
            result.append(line)
    return result


def get_opcodes(encoding_path: Path) -> dict[str, str]:
    """Extract opcode for each instruction from encoding.h."""
    content = encoding_path.read_text()
    opcodes = {}
    for m in re.finditer(r'DECLARE_INSN\((\w+),\s*(MATCH_\w+),\s*MASK_\w+\)', content):
        opcodes[m.group(1)] = m.group(2)
    return opcodes


def main():
    if len(sys.argv) < 5:
        print("Usage: gen_riscv_insns.py <riscv_dir> <encoding.h> <insn_template.cc> <output_dir>")
        sys.exit(1)

    riscv_dir = Path(sys.argv[1])
    encoding_path = Path(sys.argv[2])
    template_path = Path(sys.argv[3])
    output_dir = Path(sys.argv[4])

    insn_list_path = riscv_dir / 'riscv_insn_list.txt'
    insn_list = load_insn_list(insn_list_path)
    opcodes = get_opcodes(encoding_path)
    template = template_path.read_text()

    output_dir.mkdir(parents=True, exist_ok=True)

    # Generate insn_list.h
    insn_list_lines = []
    for insn in insn_list:
        insn_norm = insn.replace('.', '_')
        insn_list_lines.append(f'DEFINE_INSN({insn_norm})')
    (output_dir / 'insn_list.h').write_text('\n'.join(insn_list_lines) + '\n')

    # Generate each instruction .cc file
    insns_dir = riscv_dir / 'insns'
    for insn in insn_list:
        opcode = opcodes.get(insn)
        if not opcode:
            # Some instructions might have different names in encoding.h
            opcode = opcodes.get(insn.replace('.', '_'), 'MATCH_ADD')  # fallback
        if not opcode:
            print(f"Warning: no opcode for {insn}", file=sys.stderr)

        insn_cc = template.replace('NAME', insn).replace('OPCODE', opcode or 'MATCH_ADD')
        (output_dir / f'{insn}.cc').write_text(insn_cc)

    # Write file list for Meson
    (output_dir / 'generated_files.txt').write_text('\n'.join(f'{i}.cc' for i in insn_list))

    print(f'Generated {len(insn_list)} instruction files')


if __name__ == '__main__':
    main()
