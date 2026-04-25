import argparse
import os
import sys

import yaml

from chaotic import error as chaotic_error
from chaotic_openapi.back.cpp.client import renderer as client_renderer
from chaotic_openapi.back.cpp.client import translator as client_translator
from chaotic_openapi.back.cpp.handler import renderer as handler_renderer
from chaotic_openapi.back.cpp.handler import translator as handler_translator
from chaotic_openapi.front import parser as front_parser
from chaotic_openapi.front import ref_resolver


def main():
    try:
        do_main()
    except chaotic_error.BaseError as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)


def do_main():
    args = parse_args()

    # sort
    contents = {}
    for file in args.files:
        with open(file) as ifile:
            content = yaml.safe_load(ifile)
        contents[file] = content
    sorted_contents = ref_resolver.sort_openapis(contents)

    # parse
    parser = front_parser.Parser(args.name)
    for file, content in sorted_contents:
        parser.parse_schema(content, file, os.path.basename(file))

    ctx = client_renderer.Context(
        generate_path='',
        clang_format_bin=args.clang_format,
        uservices_library_tvm_guard_hack=False,
    )

    if args.gen == 'client':
        spec = client_translator.Translator(
            parser.service(),
            dynamic_config=args.dynamic_config,
            cpp_namespace=(args.namespace or f'clients::{args.name}'),
            include_dirs=args.include_dirs or [],
            middleware_plugins=[],
        ).spec()
        outputs = client_renderer.render(spec, ctx)
        client_renderer.CppOutput.save(outputs, args.output_dir)

    elif args.gen == 'handlers':
        spec = handler_translator.HandlerTranslator(
            parser.service(),
            cpp_namespace=(args.namespace or f'handlers::{args.name}'),
            include_dirs=args.include_dirs or [],
        ).spec()
        outputs = handler_renderer.render(spec, ctx)
        client_renderer.CppOutput.save(outputs, args.output_dir)

    else:
        print(f'Unknown --gen value: {args.gen!r}', file=sys.stderr)
        sys.exit(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--name', required=True, help='Client/Service name')
    parser.add_argument('-o', '--output-dir', required=True)
    parser.add_argument('--namespace', required=False)
    parser.add_argument(
        '--gen',
        choices=['client', 'handlers'],
        default='client',
        help='What to generate: client code or handler base classes (default: client)',
    )
    parser.add_argument('--dynamic-config', default='')
    parser.add_argument(
        '--clang-format',
        default='clang-format',
        help=(
            'clang-format binary name. Can be either binary name in $PATH or '
            'full filepath to a binary file. Set to empty for no formatting.'
        ),
    )
    parser.add_argument(
        '-I',
        '--include-dir',
        dest='include_dirs',
        action='append',
        help='Path to search for include files for x-usrv-cpp-type',
    )
    parser.add_argument(
        'files',
        nargs='+',
        help='openapi/swagger yaml/json schemas',
    )
    return parser.parse_args()


if __name__ == '__main__':
    main()
