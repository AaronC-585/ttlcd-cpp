#!/usr/bin/env python3
from pathlib import Path
import sys

def main() -> None:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.ttf> <output.hpp>", file=sys.stderr)
        sys.exit(1)

    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    data = src.read_bytes()

    lines = [
        "#pragma once",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace EmbeddedFont {",
        f"inline constexpr std::size_t times_new_roman_ttf_size = {len(data)};",
        "inline const unsigned char times_new_roman_ttf[] = {",
    ]
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 16])
        lines.append(f"    {chunk},")
    lines.extend(["};", "", "}  // namespace EmbeddedFont", ""])
    out.write_text("\n".join(lines))

if __name__ == "__main__":
    main()
