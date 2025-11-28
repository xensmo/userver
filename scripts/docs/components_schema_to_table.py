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


def merge_descriptions(yaml, other):
    if yaml.get('description') and other.get('description'):
        description = yaml['description'].strip()
        if not description.endswith('.'):
            description += '.'
        description += ' <i>Each of the elements:</i> ' + other['description']

        value_merged = yaml.copy()
        value_merged['description'] = description
        return value_merged
    elif yaml.get('description'):
        return yaml
    else:
        return other


def visit_object(yaml, prefix: str = ''):
    properties = yaml.get('properties', {})

    for key, value in properties.items():
        if value.get('type') == 'array':
            if value['items'].get('type') == 'object':
                yield from visit_object(value['items'], prefix + key + '.[].')
            else:
                value_merged = merge_descriptions(value, value['items'])
                yield (prefix + key + '.[]', value_merged)
        elif value.get('type') == 'object':
            yield from visit_object(value, prefix + key + '.')
        else:
            yield (prefix + key, value)

    additionals = yaml.get('additionalProperties')
    if additionals is True:
        yield (prefix + '*', yaml)
    elif not additionals:
        return
    elif additionals.get('type') == 'object':
        yield from visit_object(yaml['additionalProperties'], prefix + '*.')
    elif additionals.get('type') == 'array':
        value_merged = merge_descriptions(yaml, additionals['items'])
        yield (prefix + '*.[]', value_merged)
    else:
        value_merged = merge_descriptions(yaml, additionals)
        yield (prefix + '*', value_merged)


def format_schema(yaml) -> str:
    result = ''
    key_values = list(visit_object(yaml))
    if not key_values:
        result += 'No options'
        return result

    max_key_size = max(len(key) for key, _ in key_values)
    max_value_size = max(len(value['description']) for _, value in key_values)

    result += f'{"Name":{max_key_size}} | {"Description":{max_value_size}} | Default value\n'
    result += f'{"":-<{max_key_size}} | {"":-<{max_value_size}} | {"":-<16}\n'

    for key, value in key_values:
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
