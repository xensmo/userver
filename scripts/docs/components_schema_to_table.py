#!/usr/bin/python

import argparse
import os
import pathlib

import yaml

USERVER_ROOT = pathlib.Path(__file__).parent.parent.parent


def normalize_default_value(data) -> str:
    if data is True:
        return 'true'
    elif data is False:
        return 'false'
    else:
        return data


def format_schema(yaml) -> str:
    result = ''
    properties = yaml.get('properties', {})
    if not properties:
        result += 'No options'
        return result

    max_key_size = max(len(key) for key in properties.keys())
    max_value_size = max(len(value['description']) for value in properties.values())

    result += f'{"Name":{max_key_size}} | {"Description":{max_value_size}} | Default value\n'
    result += f'{"":-<{max_key_size}} | {"":-<{max_value_size}} | {"":-<16}\n'

    for key, value in properties.items():
        key = key.replace('\n', ' ')
        description = value['description'].replace('\n', ' ')
        default = normalize_default_value(value.get('defaultDescription', '--'))
        result += f'{key:{max_key_size}} | {description:{max_value_size}} | {default}\n'

    return result


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o')
    parser.add_argument('yaml', nargs='+')
    return parser.parse_args()


def handle_file(ifname: pathlib.Path, output_dir: pathlib.Path):
    with open(ifname) as ifile:
        content = yaml.load(ifile.read(), yaml.Loader)

    md = format_schema(content)

    output_path = output_dir / ifname.parent.relative_to(USERVER_ROOT)
    os.makedirs(str(output_path), exist_ok=True)

    output_file = output_path / (pathlib.Path(ifname).stem + '.md')
    print(output_file)

    with open(output_file, 'w') as ofile:
        ofile.write(md)


def main():
    args = parse_args()
    for fname in args.yaml:
        try:
            handle_file(pathlib.Path(fname), pathlib.Path(args.o))
        except yaml.scanner.ScannerError as err:
            print(f'Failed to parse YAML from file {fname}: {err}')


if __name__ == '__main__':
    main()
