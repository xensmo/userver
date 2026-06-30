import argparse
from dataclasses import dataclass
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

TESTS_DIR = Path(__file__).parent
INPUT_DIR = TESTS_DIR / 'input'
OUTPUT_DIR = TESTS_DIR / 'output'
MAIN_SCRIPT = TESTS_DIR.parent.parent / 'main.py'
SIMPLE_STYLE = '{BasedOnStyle: Google, ColumnLimit: 120}'

USE_COLOR = sys.stdout.isatty()
GREEN = '\033[32m' if USE_COLOR else ''
RED = '\033[31m' if USE_COLOR else ''
YELLOW = '\033[33m' if USE_COLOR else ''
BOLD = '\033[1m' if USE_COLOR else ''
RESET = '\033[0m' if USE_COLOR else ''


@dataclass
class TestCase:
    input_dir: Path
    output_dir: Path
    generate_dir: Path
    clang_format: str

    @property
    def args(self) -> list[str]:
        assert self.input_dir.is_dir()
        assert self.output_dir.is_dir()
        assert self.generate_dir.is_dir()

        migrations_dir: Path = self.input_dir / 'migrations'
        queries_dir: Path = self.input_dir / 'queries'

        args: list[str] = [
            '--dialect=postgresql',
            f'--namespace={self.input_dir.stem}',
            f'--output-dir={self.output_dir}',
            f'--clang-format={self.clang_format}',
            f'--dump-dir={self.input_dir}',
        ]

        if migrations_dir.exists():
            assert migrations_dir.is_dir()
            args.append(f'--migrations-dir={migrations_dir}')
            args.append(f'--migrations-output-dir={migrations_dir}')

        if queries_dir.exists():
            assert queries_dir.is_dir()
            args.append(f'--queries-dir={queries_dir}')

        return args

    def run(self, python_path: str) -> None:
        cmd: list[str] = [python_path, str(MAIN_SCRIPT), *self.args]
        subprocess.run(cmd, check=True)


def is_binary_available(name: str) -> bool:
    return shutil.which(name) is not None


def get_test_cases() -> list[str]:
    """Get list of test case names from input directory."""
    return sorted(d.name for d in INPUT_DIR.iterdir() if d.is_dir())


def walk_xpp(root: Path) -> list[str]:
    extensions = ('.hpp', '.cpp', '.ipp')

    return [str(path) for path in root.rglob('*') if path.is_file() and path.suffix.lower() in extensions]


def normalize_formatting(root: Path, clang_format: str) -> None:
    files = walk_xpp(root)
    if not files:
        return
    subprocess.check_call([clang_format, '--style', SIMPLE_STYLE, '-i', *files])


def compare_dirs(golden_dir: Path, generated_dir: Path, clang_format: str) -> tuple[int, str]:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        golden_copy = tmp_path / 'golden'
        generated_copy = tmp_path / 'generated'
        shutil.copytree(golden_dir, golden_copy)
        shutil.copytree(generated_dir, generated_copy)

        normalize_formatting(golden_copy, clang_format)
        normalize_formatting(generated_copy, clang_format)

        result = subprocess.run(
            ['diff', '-uNrpB', str(golden_copy), str(generated_copy)],
            capture_output=True,
            text=True,
        )
        return result.returncode, result.stdout + result.stderr


def canonize_case(case_name: str, clang_format: str, python_path: str) -> None:
    golden_dir = OUTPUT_DIR / case_name
    if golden_dir.exists():
        shutil.rmtree(golden_dir)
    golden_dir.mkdir(parents=True)

    TestCase(
        input_dir=INPUT_DIR / case_name,
        output_dir=golden_dir,
        generate_dir=golden_dir,
        clang_format=clang_format,
    ).run(python_path)


def verify_case(case_name: str, clang_format: str, python_path: str) -> tuple[int, str]:
    golden_dir = OUTPUT_DIR / case_name
    if not golden_dir.exists():
        return 1, f'no golden files for {case_name} (run with --canonize-tests to create them)\n'

    with tempfile.TemporaryDirectory() as tmp:
        generated_dir = Path(tmp)
        TestCase(
            input_dir=INPUT_DIR / case_name,
            output_dir=generated_dir,
            generate_dir=generated_dir,
            clang_format=clang_format,
        ).run(python_path)
        return compare_dirs(golden_dir, generated_dir, clang_format)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-Z',
        '--canonize-tests',
        action='store_true',
        help='Regenerate golden files instead of comparing.',
    )
    parser.add_argument(
        '--clang-format',
        default='clang-format',
        help='clang-format binary name used both by sqldto and by compare-time normalization.',
    )
    parser.add_argument(
        '--python',
        default=sys.executable,
        help='Python interpreter used to invoke sqldto main.py.',
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not is_binary_available(args.clang_format):
        print(f'Error: {args.clang_format} is not available in PATH', file=sys.stderr)
        return 1

    cases = get_test_cases()
    if not cases:
        print(f'no test cases found in {INPUT_DIR}', file=sys.stderr)
        return 1

    if args.canonize_tests:
        for case_name in cases:
            canonize_case(case_name, args.clang_format, args.python)
            print(f'[{case_name}] {BOLD}{YELLOW}Re-write{RESET}')
        return 0

    if not is_binary_available('diff'):
        print('Error: diff is not available in PATH', file=sys.stderr)
        return 1

    failed = False
    for case_name in cases:
        rc, diff_text = verify_case(case_name, args.clang_format, args.python)
        if rc != 0:
            failed = True
            print(f'[{case_name}] {BOLD}{RED}FAIL{RESET}')
            if diff_text:
                print(diff_text)
        else:
            print(f'[{case_name}] {BOLD}{GREEN}OK{RESET}')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
