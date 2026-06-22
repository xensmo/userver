import argparse
import enum
import logging
import pathlib
import sys

from sqldto.common import errors
from sqldto.postgres.generator import PgGenerator


class Dialect(str, enum.Enum):
    postgresql = 'postgresql'

    @staticmethod
    def parse(arg: str):
        try:
            return Dialect(arg)
        except ValueError:
            raise errors.ClientError(f'Unknown dialect: {arg}')


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--namespace',
        type=str,
        required=True,
        help='c++ namespace to use',
    )
    parser.add_argument(
        '--output-dir',
        type=pathlib.Path,
        required=True,
        help='path to the directory with .hpp-.cpp files to generate',
    )
    parser.add_argument(
        '--dump-dir',
        type=pathlib.Path,
        required=True,
        help='path to the directory with schema dumps',
    )
    parser.add_argument(
        '--dialect',
        type=Dialect.parse,
        default=Dialect.postgresql,
        choices=[Dialect.postgresql],
        required=False,
        help='dialect to use (default: postgresql)',
    )
    parser.add_argument(
        '--migrations-dir',
        type=pathlib.Path,
        required=True,
        help='path to the directory with migration files',
    )
    parser.add_argument(
        '--migrations-output-dir',
        type=pathlib.Path,
        required=False,
        help='path to the directory for code-generated migration files',
    )
    parser.add_argument(
        '--queries-dir',
        type=pathlib.Path,
        required=False,
        help='path to the directory with queries files',
    )
    parser.add_argument(
        'queries',
        nargs='*',
        help='input query files to process (relative to the cwd, not source-dir)',
    )
    parser.add_argument(
        '--clang-format',
        type=str,
        default='clang-format',
        required=False,
        help='clang-format binary name. Set to empty for no formatting',
    )
    parser.add_argument(
        '--dump',
        action='store_true',
        help='fetch the DB schema and write it to the schema dump json, then exit (no code generation)',
    )
    args = parser.parse_args()

    if args.queries_dir and not args.queries:
        args.queries = [str(path) for path in args.queries_dir.rglob('*.sql')]

    return args


def dump_command() -> str:
    script = pathlib.Path(__file__).resolve()
    args = [arg for arg in sys.argv[1:] if arg != '--dump']
    return ' '.join([sys.executable, str(script), *args, '--dump'])


def pg_run(args: argparse.Namespace) -> None:
    generator = PgGenerator(
        namespace=args.namespace,
        output_dir=args.output_dir,
        dump_dir=args.dump_dir,
        clang_format=args.clang_format,
        migrations_dir=args.migrations_dir,
        queries_dir=args.queries_dir,
        query_files=[filename for filename in args.queries],
        migrations_output_dir=args.migrations_output_dir,
        regen_command=dump_command(),
    )

    if args.dump:
        generator.dump()
    else:
        generator.generate()


def run(args: argparse.Namespace) -> None:
    {
        Dialect.postgresql: pg_run,
    }[args.dialect](args)


def main() -> int:
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)

    try:
        run(parse_args())
        return 0

    except errors.UnexpectedError:
        raise

    except errors.BaseError as e:
        print('#' * 80)
        print(e, file=sys.stderr)
        print('#' * 80)
        return 1


if __name__ == '__main__':
    sys.exit(main())
