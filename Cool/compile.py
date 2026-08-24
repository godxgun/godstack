#!/usr/bin/env python3
import sys
import re
import argparse

TYPE_SPECIFIERS = {
    'int': '%d',
    'int32_t': '%d',
    'short': '%d',
    'long': '%ld',
    'unsigned': '%u',
    'unsigned int': '%u',
    'uint32_t': '%u',
    'size_t': '%zu',
    'float': '%f',
    'double': '%f',
    'char': '%c',
    'char*': '%s',
    'const char*': '%s',
    'char *': '%s',
    'const char *': '%s',
}

def parse_parameters(args_str: str) -> dict:
    param_map = {}
    if not args_str or args_str == 'void':
        return param_map

    params = args_str.split(',')
    for param in params:
        param = param.strip()
        if not param:
            continue
        
        match = re.match(r'^(.*?)\b([A-Za-z0-9_]+)$', param)
        if match:
            type_part = match.group(1).strip()
            var_name = match.group(2).strip()
            
            type_clean = re.sub(r'\s*\*\s*', '*', type_part)
            specifier = TYPE_SPECIFIERS.get(type_clean, '%d')
            param_map[var_name] = (type_clean, specifier)
            
    return param_map

def escape_c_string(text: str) -> str:
    return (text.replace('\\', '\\\\')
                .replace('"', '\\"')
                .replace('\r', '')
                .replace('\n', '\\n'))

def transpile_cool(cool_code: str, filename: str = "<stdin>") -> str:
    lines = cool_code.splitlines(keepends=True)
    c_output = []
    
    cool_func_re = re.compile(r'^\s*COOL\s+void\s+([A-Za-z0-9_]+)\s*\((.*?)\)\s*\{\s*$')
    
    in_cool_func = False
    current_func = ""
    raw_buffer = []
    local_scope = {}

    def flush_raw_buffer():
        nonlocal raw_buffer
        if raw_buffer:
            combined = "".join(raw_buffer)
            if combined:
                escaped = escape_c_string(combined)
                c_output.append(f'    cool_html_raw(COOL_SV("{escaped}"));\n')
            raw_buffer = []

    for line_num, line in enumerate(lines, start=1):
        match = cool_func_re.match(line)
        
        if match and not in_cool_func:
            in_cool_func = True
            current_func, args = match.groups()
            args_str = args.strip() if args.strip() else "void"
            
            local_scope = parse_parameters(args_str)
            c_output.append(f"void {current_func}({args_str}) {{\n")
            continue

        if in_cool_func:
            if line.strip() == "}":
                flush_raw_buffer()
                c_output.append("}\n\n")
                in_cool_func = False
                current_func = ""
                local_scope = {}
                continue

            # Tokenize line into @Component(), { expression }, and plain HTML
            tokens = re.split(r'(@[A-Za-z0-9_]+\s*\([^)]*\)|\{[^{}]+\})', line)

            for token in tokens:
                if not token:
                    continue
                
                # @Component()
                if token.startswith('@') and '(' in token:
                    flush_raw_buffer()
                    call_code = token[1:].strip()
                    c_output.append(f"    {call_code};\n")
                
                # { expression }
                elif token.startswith('{') and token.endswith('}'):
                    flush_raw_buffer()
                    expr = token[1:-1].strip()
                    
                    if expr in local_scope:
                        type_clean, specifier = local_scope[expr]
                        
                        if 'char*' in type_clean:
                            c_output.append(f"    cool_html_txt({expr}, strlen({expr}));\n")
                        else:
                            c_output.append(f'    cool_htmlf_raw("{specifier}", {expr});\n')
                    else:
                        available = ", ".join(f"'{p}'" for p in local_scope.keys()) if local_scope else "none"
                        sys.stderr.write(
                            f"\n[Transpiler Error] {filename}:{line_num}: "
                            f"Expression '{{{expr}}}' in function '{current_func}' does not match any parameter.\n"
                            f"  -> Available parameter(s): {available}\n\n"
                        )
                        sys.exit(1)
                
                # Raw HTML text
                else:
                    raw_buffer.append(token)
        else:
            c_output.append(line)

    return "".join(c_output)

def main():
    parser = argparse.ArgumentParser(description="Transpiles .cool template files to C source code.")
    parser.add_argument("input", help="Input .cool file")
    parser.add_argument("-o", "--output", help="Output .c file")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        cool_code = f.read()

    transpiled_c = transpile_cool(cool_code, filename=args.input)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(transpiled_c)
    else:
        sys.stdout.write(transpiled_c)

if __name__ == "__main__":
    main()
