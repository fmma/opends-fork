#!/usr/bin/env python3
"""Generate read_cases.h from read_cases.txt."""
import sys


def load_cases(path):
    cases = []
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            offset, size = line.split()
            cases.append((int(offset), int(size)))
    return cases


def main():
    cases = load_cases(sys.argv[1])

    with open(sys.argv[2], "w") as out:
        out.write(
            "/* Generated from tests/read_cases.txt"
            " -- do not edit. */\n"
            "#ifndef READ_CASES_H_\n"
            "#define READ_CASES_H_\n"
            "\n"
            "#include <stddef.h>\n"
            "#include <sys/types.h>\n"
            "\n"
            "struct test_case {\n"
            "\toff_t offset;\n"
            "\tsize_t size;\n"
            "};\n"
            "\n"
            "static const struct test_case cases[] = {\n"
        )

        for offset, size in cases:
            out.write(f"\t{{{offset:>5}, {size:>5}}},\n")

        out.write(
            "};\n"
            "\n"
            "#define NCASES (sizeof(cases) / sizeof(cases[0]))\n"
            "\n"
            "static inline size_t read_max_size(void)\n"
            "{\n"
            "\tsize_t max = 0;\n"
            "\tfor (size_t i = 0; i < NCASES; i++) {\n"
            "\t\tif (cases[i].size > max)\n"
            "\t\t\tmax = cases[i].size;\n"
            "\t}\n"
            "\treturn max;\n"
            "}\n"
            "\n"
            "#endif /* READ_CASES_H_ */\n"
        )


if __name__ == "__main__":
    main()
