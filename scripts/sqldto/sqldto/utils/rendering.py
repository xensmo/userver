from dataclasses import dataclass
import pathlib

import jinja2

from sqldto.utils import cpp_format


@dataclass
class ToGenerate:
    template_name: str
    output_file: pathlib.Path
    context: dict
    clang_format: bool = False


class GeneratorBase:
    def __init__(
        self,
        templates_dir: pathlib.Path,
        clang_format: str | None = None,
    ):
        self.templates_dir = templates_dir
        self.clang_format = clang_format or ''
        self.env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(str(self.templates_dir)),
        )

    def render(self, to_generate: ToGenerate) -> None:
        template = self.env.get_template(to_generate.template_name)
        content = template.render(to_generate.context)

        if to_generate.clang_format:
            content = cpp_format.format_pp(
                content,
                binary=self.clang_format,
                output_file=to_generate.output_file,
            )

        to_generate.output_file.parent.mkdir(parents=True, exist_ok=True)
        to_generate.output_file.write_text(content, encoding='utf-8')
