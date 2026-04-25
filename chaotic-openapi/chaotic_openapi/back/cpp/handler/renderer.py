from pathlib import Path
import re

import jinja2

from chaotic import cpp_format
from chaotic import jinja_env
from chaotic.back.cpp import renderer as cpp_renderer
from chaotic_openapi.back.cpp.client.renderer import Context
from chaotic_openapi.back.cpp.client.renderer import CppOutput
from chaotic_openapi.back.cpp.common.types import Operation
from chaotic_openapi.back.cpp.handler.types import ServerSpec

PARENT_DIR = Path(__file__).parent

_UPPER_RE = re.compile(r'([A-Z])')
_BRACES_RE = re.compile(r'[{}]')
_MULTI_DASH_RE = re.compile(r'-+')

HANDLER_TEMPLATE_NAMES = [
    'requests.hpp',
    'requests.cpp',
    'responses.hpp',
    'responses.cpp',
    'handler.hpp',
    'handler.cpp',
]


def _to_kebab(s: str) -> str:
    s = _UPPER_RE.sub(r'-\1', s)
    return s.replace('_', '-').lower().lstrip('-')


def _path_slug(path: str) -> str:
    slug = _BRACES_RE.sub('', path).strip('/').replace('/', '-')
    slug = _MULTI_DASH_RE.sub('-', slug)
    return slug.lower()


def component_name(op: Operation) -> str:
    if op.operation_id:
        return 'handler-' + _to_kebab(op.operation_id)
    slug = _path_slug(op.path) + '-' + op.method.lower()
    return 'handler-' + slug


def make_op_relpath(op: Operation) -> str:
    if op.operation_id:
        return op.operation_id.lower()
    path = op.path.strip('/').replace('/', '_')
    return f'{path}/{op.method.lower()}'


def make_env() -> jinja2.Environment:
    env = jinja_env.make_env(
        'chaotic-openapi/chaotic_openapi/back/cpp/handler',
        str(PARENT_DIR),
    )
    env.globals['list'] = list
    env.globals['enumerate'] = enumerate
    env.globals['str'] = str
    env.globals['component_name'] = component_name
    env.globals['make_op_relpath'] = make_op_relpath
    return env


JINJA_ENV = make_env()


def render(spec: ServerSpec, context: Context) -> list[CppOutput]:
    output = []

    for op in spec.operations:
        env = {'spec': spec, 'op': op}

        for name in HANDLER_TEMPLATE_NAMES:
            tpl = JINJA_ENV.get_template(f'templates/{name}.jinja')
            pp = tpl.render(**env)
            pp = cpp_format.format_pp(pp, binary=context.clang_format_bin)

            op_path = f'handlers/{spec.service_name}/{make_op_relpath(op)}'
            if name.endswith('.hpp'):
                rel_path = f'include/{op_path}/{name}'
            else:
                rel_path = f'src/{op_path}/{name}'

            output.append(CppOutput(rel_path=rel_path, content=pp))

    # Schema type files are service-level (shared across all operations)
    vfilepath_map = {}
    for cpp_type in spec.extract_cpp_types().values():
        assert cpp_type.json_schema
        filepath = cpp_type.json_schema.source_location().filepath
        vfilepath_map[filepath] = 'handlers/{}/{}'.format(spec.service_name, filepath)

    r = cpp_renderer.OneToOneFileRenderer(
        relative_to='',
        vfilepath_to_relfilepath=vfilepath_map,
        clang_format_bin=context.clang_format_bin,
        generate_serializer=True,
        generate_sax_parser=True,
    )
    cpp_outputs = r.render(
        spec.extract_cpp_types(),
        local_pair_header=False,
    )
    for cpp_output in cpp_outputs:
        for file in cpp_output.files:
            output.append(
                CppOutput(
                    rel_path=str(Path(file.subdir) / (cpp_output.filepath_wo_ext + file.ext)),
                    content=file.content,
                ),
            )

    return output
