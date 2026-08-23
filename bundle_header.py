import re
import sys
from pathlib import Path

def bundle_header(input_path: Path):
    """
    Looks for #include "filename" // <REPLACE>
    And inlines that file directly into the code.
    """
    output_path = input_path.with_suffix(input_path.suffix + ".out")
    
    include_pattern = re.compile(r'#include\s+["<](?P<filename>.*)[">]\s*//\s*<REPLACE>')

    def process_file(file_path: Path) -> str:
        if not file_path.exists():
            print(f"Warning: {file_path} not found.")
            return f"// Error: {file_path} not found\n"

        lines = []
        with open(file_path, 'r') as f:
            for line in f:
                match = include_pattern.search(line)
                if match:
                    include_name = match.group('filename')
                    # Look for the file relative to the current file's directory
                    nested_path = file_path.parent / include_name
                    lines.append(f"/* --- Start of {include_name} --- */\n")
                    lines.append(process_file(nested_path))
                    lines.append(f"/* --- End of {include_name} --- */\n")
                else:
                    lines.append(line)
        return "".join(lines)

    content = process_file(input_path)
    
    with open(output_path, 'w') as f:
        f.write(content)
    
    print(f"Successfully bundled into: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python build.py <path_to_header>")
        sys.exit(1)
    
    bundle_header(Path(sys.argv[1]))
