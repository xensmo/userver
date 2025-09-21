"""Graph of dependencies between types and a topology sort for that graph."""

from __future__ import annotations

from collections.abc import Iterable
from collections.abc import Sequence
import dataclasses
import graphlib
import typing
from typing import Dict
from typing import List
from typing import MutableSet
from typing import Tuple
from typing import TypeVar

from proto_structs.models import gen_node
from proto_structs.models import toposort
from proto_structs.models import type_ref
from proto_structs.models import type_ref_consts

#: Graph nodes are direct children of the scope (namespace or struct) that need to be sorted.
#:
#: "Node A depends on node B" means: a type defined in A (possibly A itself) refers to
#: a type defined in B (possibly B itself).
#:
#: Guideline: write the type of some `CodegenNode` as `_GraphNode` if and only if it is directly a graph node
#: (not a nested type, not an unrelated type).
_GraphNode = gen_node.CodegenNode

_MAX_CYCLE_BREAK_ATTEMPTS = 1000


@dataclasses.dataclass
class _Cycle:
    #: A list of nodes forming a cycle, such that each node depends on the next node in the list.
    #: The last node is guaranteed to be a duplicate of the first node, to make it clear that the list is cyclic.
    nodes: List[_GraphNode]

    @staticmethod
    def from_exception(exception: graphlib.CycleError) -> _Cycle:
        """Extracts cycle info from exception."""
        cycle: List[_GraphNode] = exception.args[1]
        assert len(cycle) >= 2
        assert cycle[-1] is cycle[0]
        return _Cycle(nodes=list(reversed(cycle)))

    @property
    def edges(self) -> Iterable[Tuple[_GraphNode, _GraphNode]]:
        """Graph edges (node -> dependency) that form the cycle."""
        return zip(self.nodes[:-1], self.nodes[1:])


def _iter_struct_fields(node: gen_node.CodegenNode) -> Iterable[gen_node.StructField]:
    for child in node.iter_all_children():
        if isinstance(child, (gen_node.StructNode, gen_node.OneofNode)):
            yield from child.fields


class _Graph:
    def __init__(self, nodes: Sequence[_GraphNode]) -> None:
        # The nodes to sort. These are direct children of the scope (namespace or struct) that needs to be sorted.
        self._nodes = nodes
        # Allows to go from `TypeReference` to `TypeNode` for references to generated types.
        self._types_by_names: Dict[str, gen_node.TypeNode] = {
            child.full_cpp_name(): child for node in nodes for child in gen_node.iter_type_nodes(node)
        }
        # A map from all nested type definitions (including self) to their (possibly indirect) parents from `nodes`.
        self._types_to_containing_graph_nodes: Dict[gen_node.TypeNode, _GraphNode] = {
            child: node for node in nodes for child in gen_node.iter_type_nodes(node)
        }
        # Maps nodes to their dependencies, as required by `TopologicalSorter`.
        self._graph: Dict[_GraphNode, MutableSet[_GraphNode]] = {}

    def prepare(self) -> None:
        for node in self._nodes:
            self._graph.setdefault(node, set())

        for node in self._nodes:
            for dep in self.iter_referenced_type_nodes(node, ignore_types_strictly_inside=node):
                self._graph[node].add(self._types_to_containing_graph_nodes[dep])

    def iter_referenced_type_nodes(
        self,
        entity: type_ref.HasTypeDependencies,
        *,
        ignore_types_strictly_inside: _GraphNode,
    ) -> Iterable[gen_node.TypeNode]:
        return (
            dep_node
            for dep in entity.type_dependencies()
            if dep.kind == type_ref.TypeDependencyKind.STRONG
            # Select `TypeReference`s to types defined among `self._nodes`, get the corresponding `TypeNode`s.
            if (dep_node := self._types_by_names.get(dep.type_reference.full_cpp_name()))
            # Dependency within a graph node will be resolved while sorting the node itself.
            # However, cyclic dependency on the graph node itself needs to be accounted for.
            if self._types_to_containing_graph_nodes[dep_node] is not ignore_types_strictly_inside
            or dep_node is ignore_types_strictly_inside
        )

    def can_break_dependency(self, node: _GraphNode, *, dependency: _GraphNode) -> bool:
        # Can break dependency if node only depends on the type itself, not on its nested types.
        # This is because nested types cannot be forward declared.
        return all(
            dep_node is dependency
            for dep_node in self.iter_referenced_type_nodes(node, ignore_types_strictly_inside=node)
            if self._types_to_containing_graph_nodes[dep_node] is dependency
        )

    def break_dependency(self, node: _GraphNode, *, dependency: _GraphNode) -> None:
        for field in _iter_struct_fields(node):
            for dep_node in self.iter_referenced_type_nodes(field.field_type, ignore_types_strictly_inside=node):
                if self._types_to_containing_graph_nodes[dep_node] is dependency:
                    if dep_node is dependency:
                        gen_node.wrap_field_in_box(field)
                    else:
                        field.field_type = type_ref_consts.UNBREAKABLE_DEPENDENCY_CYCLE

    def sort_nodes(self) -> List[_GraphNode]:
        attempts = 0
        while True:
            try:
                # Need to at least ensure codegen output stability.
                # Keeping the order close to the original proto file is also helpful.
                return toposort.stable_toposort(items=self._nodes, graph=self._graph)
            except graphlib.CycleError as exc:
                cycle = _Cycle.from_exception(exc)

            if attempts == _MAX_CYCLE_BREAK_ATTEMPTS:
                names = (node.full_cpp_name() for node in cycle.nodes)
                raise RuntimeError(f'Failed to break dependency cycle: {" -> ".join(names)}')
            attempts += 1

            can_break_dependencies = [
                self.can_break_dependency(node, dependency=dependency) for node, dependency in cycle.edges
            ]
            can_break_any_dependency = any(can_break_dependencies)
            for (node, dependency), can_break in zip(cycle.edges, can_break_dependencies):
                if can_break or not can_break_any_dependency:
                    self.break_dependency(node, dependency=dependency)
                    self._graph[node].discard(dependency)


Node = TypeVar('Node', bound=gen_node.CodegenNode)


def sort_nodes_topologically(nodes: Sequence[Node]) -> List[Node]:
    """
    Topologically sort `nodes`.
    Wrap fields in `utils::Box` (as in `gen_node.wrap_field_in_box`) when necessary.
    As a last resort, replace the field type with `proto_structs::UnbreakableDependencyCycle`.
    """
    graph = _Graph(nodes)
    graph.prepare()
    sorted_nodes = graph.sort_nodes()
    # The result is a permutation of `nodes`, so the item type remains the same.
    return typing.cast(List[Node], sorted_nodes)
