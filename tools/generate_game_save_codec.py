#!/usr/bin/env python3
"""Generate the field-by-field GameSession codec included by game_save.c.

Generation uses pycparser and the configured GCC preprocessor.  The generated
file is checked in, so neither dependency is needed by normal builds.  When
session structs change, regenerate the codec and bump the on-disk version in
game_save.c.
"""

from __future__ import annotations

import argparse
import operator
import shutil
import subprocess
import tempfile
from pathlib import Path

from pycparser import c_ast, c_parser


ROOT = Path(__file__).resolve().parents[1]
STUB_STDINT = """\
typedef signed char int8_t; typedef unsigned char uint8_t;
typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
typedef unsigned long long uintptr_t;
"""
STUB_STDDEF = "typedef unsigned long long size_t;\n"
ARRAY_CONSTANTS = {}


def type_node(node):
    while isinstance(node, (c_ast.TypeDecl, c_ast.ArrayDecl)):
        node = node.type
    return node


def array_parts(node):
    dimensions = []
    while isinstance(node, c_ast.ArrayDecl):
        dimensions.append(eval_constant(node.dim))
        node = node.type
    return dimensions, node


def eval_constant(node):
    if isinstance(node, c_ast.Constant):
        return int(node.value.rstrip("uUlL"), 0)
    if isinstance(node, c_ast.ID) and node.name in ARRAY_CONSTANTS:
        return ARRAY_CONSTANTS[node.name]
    if isinstance(node, c_ast.UnaryOp) and node.op in ("+", "-"):
        value = eval_constant(node.expr)
        return value if node.op == "+" else -value
    if isinstance(node, c_ast.BinaryOp):
        operators = {
            "+": operator.add, "-": operator.sub, "*": operator.mul,
            "/": operator.floordiv, "%": operator.mod,
            "<<": operator.lshift, ">>": operator.rshift,
            "|": operator.or_, "&": operator.and_, "^": operator.xor,
        }
        function = operators.get(node.op)
        if function is not None:
            return function(eval_constant(node.left), eval_constant(node.right))
    raise ValueError(f"unsupported array bound in GameSession schema: {node!r}")


class CodecGenerator:
    def __init__(self, ast):
        self.typedefs = {}
        self.aliases = {}
        global ARRAY_CONSTANTS
        ARRAY_CONSTANTS = {}
        for item in ast.ext:
            if isinstance(item, c_ast.Typedef):
                enum_node = type_node(item.type)
                if isinstance(enum_node, c_ast.Enum) and enum_node.values:
                    current = -1
                    for enumerator in enum_node.values.enumerators:
                        current = (eval_constant(enumerator.value)
                                   if enumerator.value is not None
                                   else current + 1)
                        ARRAY_CONSTANTS[enumerator.name] = current
            elif isinstance(item, c_ast.Decl) and isinstance(item.type, c_ast.Enum):
                enum_node = item.type
                if enum_node.values:
                    current = -1
                    for enumerator in enum_node.values.enumerators:
                        current = (eval_constant(enumerator.value)
                                   if enumerator.value is not None
                                   else current + 1)
                        ARRAY_CONSTANTS[enumerator.name] = current
        for item in ast.ext:
            if isinstance(item, c_ast.Typedef):
                self.typedefs[item.name] = item.type
        self.tag_names = {}
        self.tag_definitions = {}
        self._collect_tags(ast)
        for name, declaration in self.typedefs.items():
            node = type_node(declaration)
            if isinstance(node, (c_ast.Struct, c_ast.Union)):
                if not node.decls and node.name in self.tag_definitions:
                    node = self.tag_definitions[node.name]
                if node.decls:
                    self.aliases.setdefault(id(node), name)
        self.functions = {}
        self.function_order = []
        self.counter = 0

    def _collect_tags(self, ast):
        class TagVisitor(c_ast.NodeVisitor):
            def __init__(self, parent):
                self.parent = parent

            def visit_Struct(self, node):
                if node.name and node.decls:
                    self.parent.tag_names[id(node)] = ("struct", node.name)
                    self.parent.tag_definitions[node.name] = node
                self.generic_visit(node)

            def visit_Union(self, node):
                if node.name and node.decls:
                    self.parent.tag_names[id(node)] = ("union", node.name)
                    self.parent.tag_definitions[node.name] = node
                self.generic_visit(node)

        TagVisitor(self).visit(ast)

    def _new_temp(self, prefix="save_value"):
        self.counter += 1
        return f"{prefix}_{self.counter}"

    def _resolve_alias(self, node):
        node = type_node(node)
        seen = set()
        while isinstance(node, c_ast.IdentifierType) and len(node.names) == 1:
            name = node.names[0]
            if name not in self.typedefs or name in seen:
                break
            seen.add(name)
            target = type_node(self.typedefs[name])
            if (isinstance(target, (c_ast.Struct, c_ast.Union)) and
                    not target.decls and target.name in self.tag_definitions):
                target = self.tag_definitions[target.name]
            if isinstance(target, (c_ast.Struct, c_ast.Union, c_ast.Enum)):
                return name, target
            node = target
        if (isinstance(node, (c_ast.Struct, c_ast.Union)) and
                not node.decls and node.name in self.tag_definitions):
            node = self.tag_definitions[node.name]
            return self.aliases.get(id(node)), node
        return None, node

    def _helper_name(self, compound, alias=None):
        if alias is None:
            alias = self.aliases.get(id(compound))
        if alias is not None:
            return "codec_" + alias, alias
        tag = self.tag_names.get(id(compound))
        if tag is None:
            return None, None
        return "codec_tag_" + tag[1], f"{tag[0]} {tag[1]}"

    def _add_compound(self, compound, alias=None):
        name, ctype = self._helper_name(compound, alias)
        if name is None:
            return None
        if name in self.functions:
            return name
        self.functions[name] = None
        self.function_order.append(name)
        fields = compound.decls or []
        if isinstance(compound, c_ast.Union) and fields:
            fields = [max(fields, key=lambda field: self._weight(field.type))]
        body = []
        for field in fields:
            if alias == "GamePropertyPresentationState" and field.name.startswith("insurance_"):
                # save_session_valid rejects the blocking premium continuation.
                continue
            if alias == "GameCompanyIssuePresentationState" and field.name.startswith("entry_"):
                continue
            if alias == "GameSession" and field.name in (
                    "debug", "audio_events", "deferred_events",
                    "ai_selection", "rng_clock_offset"):
                continue
            if alias == "GamePresentationState" and field.name in (
                    "save_load", "auto_save_pending"):
                continue
            if alias == "GameGodAcquisitionPresentationState" and field.name in (
                    "money_pending", "waiting_for_detach", "money"):
                # 0x440706 is a blocking modal. save_session_valid rejects
                # unsettled god money, preserving the format-4 byte layout.
                continue
            if alias == "GameEndingPresentationState" and field.name in (
                    "continuation_active",):
                # The 0x407842 continuation prompt is a transient outer-loop
                # modal.  It is never writable through the save panel, and
                # omitting it preserves the existing format-3 byte layout.
                continue
            if alias == "GameBankruptcyPresentationState" and field.name in (
                    "original_play_state", "previous_current_player",
                    "post_speech_lifecycle"):
                # These are likewise only needed while the blocking
                # bankruptcy animation is alive.  Keep old format-3 saves
                # byte-for-byte compatible; the save panel cannot be opened
                # while this modal owns the frame.
                continue
            # Preserve the existing format-4..8 layout: these are runtime
            # clocks or blocking-modal continuation fields, not durable data.
            transient = {
                "GameSession": {"pending_turn_events"},
                "GamePresentationState": {"animation_clock", "special_place_notice"},
                "GamePropertyPresentationState": {"flash_presented_frame"},
                "GameEndingPresentationState": {"continuation_confirmation_frame"},
                "GameSpecialEventPresentationState": {
                    "fortune_choices_done", "fortune_defense_ready", "fortune_redirect_target",
                    "fortune_passive_card", "fortune_choice_hover", "fortune_card_targets_seen",
                    "fortune_god_notice_done", "fortune_passive_resolved"},
                "CardPanelState": {"effect_frame", "effect_frame_count", "effect_clock"},
            }
            if field.name in transient.get(alias, set()):
                continue
            emitted = self._emit_field(ctype, "value", field.name, field.type)
            if alias == "GamePointMinigamePresentationState" and field.name == "catcher":
                emitted = ["if (codec->version >= 9) {"] + [
                    "    " + line for line in emitted] + ["}"]
            if alias == "GameToolAnimationRequest" and field.name in (
                    "confinement_days", "explosion_traffic_method"):
                emitted = ["if (codec->version >= 7) {"] + [
                    "    " + line for line in emitted] + ["}"]
            if ((alias == "GameToolAnimationRequest" and field.name in (
                    "admission_stage", "origin_play_state", "origin_traffic_method")) or
                (alias == "GameSpecialEventPresentationState" and field.name == "pending_confinement_days") or
                (alias == "GameInsuranceNoticeEntry" and field.name == "cause")):
                emitted = ["if (codec->version >= 8) {"] + [
                    "    " + line for line in emitted] + ["}"]
            body.extend(emitted)
        self.functions[name] = (
            f"static void {name}(SaveCodec *codec, {ctype} *value)\n{{\n"
            + self._indent(body)
            + "}\n"
        )
        return name

    def _weight(self, node):
        dimensions, base = array_parts(node)
        multiplier = 1
        for dimension in dimensions:
            multiplier *= dimension
        alias, base = self._resolve_alias(base)
        if isinstance(base, c_ast.PtrDecl):
            return 0
        if isinstance(base, (c_ast.Struct, c_ast.Union)):
            return multiplier * sum(self._weight(f.type) for f in base.decls or [])
        if isinstance(base, c_ast.Enum):
            return multiplier
        if isinstance(base, c_ast.IdentifierType) and alias is not None:
            return self._weight(self.typedefs[alias]) * multiplier
        return multiplier

    @staticmethod
    def _indent(lines):
        return "".join("    " + line + "\n" for line in lines)

    def _emit_compound(self, owner_type, expression, compound, alias=None):
        helper, _ = self._helper_name(compound, alias)
        if helper is not None:
            self._add_compound(compound, alias)
            return [f"{helper}(codec, &({expression}));"]
        # Anonymous inline structures have no C type name.  Their members are
        # encoded in-place through the containing member expressions.
        fields = compound.decls or []
        if isinstance(compound, c_ast.Union) and fields:
            fields = [max(fields, key=lambda field: self._weight(field.type))]
        lines = []
        for field in fields:
            lines.extend(self._emit_field(
                owner_type, expression, field.name, field.type, "."))
        return lines

    def _emit_scalar(self, expression, node):
        alias, resolved = self._resolve_alias(node)
        if isinstance(resolved, c_ast.Enum):
            temp = self._new_temp("enum_value")
            ctype = alias or "int"
            return [
                f"int32_t {temp} = (int32_t)({expression});",
                f"codec_scalar(codec, &{temp}, SAVE_CODEC_I32);",
                f"if (codec->reading) ({expression}) = ({ctype}){temp};",
            ]
        if isinstance(resolved, c_ast.Struct) or isinstance(resolved, c_ast.Union):
            helper = self._add_compound(resolved, alias)
            if helper is None:
                raise ValueError(f"anonymous compound field needs inline expansion: {expression}")
            return [f"{helper}(codec, &({expression}));"]
        if not isinstance(resolved, c_ast.IdentifierType):
            raise ValueError(f"unsupported field type at {expression}: {type(resolved).__name__}")
        words = resolved.names
        spelling = " ".join(words)
        kinds = {
            "char": "SAVE_CODEC_U8", "signed char": "SAVE_CODEC_I8",
            "unsigned char": "SAVE_CODEC_U8", "int8_t": "SAVE_CODEC_I8",
            "uint8_t": "SAVE_CODEC_U8", "short": "SAVE_CODEC_I16",
            "short int": "SAVE_CODEC_I16", "signed short": "SAVE_CODEC_I16",
            "signed short int": "SAVE_CODEC_I16", "unsigned short": "SAVE_CODEC_U16",
            "unsigned short int": "SAVE_CODEC_U16", "int16_t": "SAVE_CODEC_I16",
            "uint16_t": "SAVE_CODEC_U16", "int": "SAVE_CODEC_I32",
            "signed": "SAVE_CODEC_I32", "signed int": "SAVE_CODEC_I32",
            "unsigned": "SAVE_CODEC_U32", "unsigned int": "SAVE_CODEC_U32",
            "int32_t": "SAVE_CODEC_I32", "uint32_t": "SAVE_CODEC_U32",
            "float": "SAVE_CODEC_F32", "_Bool": "SAVE_CODEC_U8",
            "long long": "SAVE_CODEC_I64", "long long int": "SAVE_CODEC_I64",
            "unsigned long long": "SAVE_CODEC_U64",
            "unsigned long long int": "SAVE_CODEC_U64",
            "int64_t": "SAVE_CODEC_I64", "uint64_t": "SAVE_CODEC_U64",
        }
        kind = kinds.get(spelling)
        if kind is None and alias is not None:
            # Primitive typedef chains such as size_t are resolved to their
            # declared base above; this branch gives a useful hard failure.
            raise ValueError(f"unsupported typedef {alias} in {expression}")
        if kind is None:
            raise ValueError(f"unsupported scalar type {spelling} in {expression}")
        return [f"codec_scalar(codec, &({expression}), {kind});"]

    def _emit_field(self, owner_type, base_expression, field_name, node,
                    member_operator="->"):
        dimensions, base = array_parts(node)
        expression = f"{base_expression}{member_operator}{field_name}"
        _, primitive = self._resolve_alias(base)
        if dimensions and isinstance(primitive, c_ast.IdentifierType):
            # Only primitive arrays are contiguous encoded fields. Never copy
            # structs, padding, enums, pointers or transient session members.
            scalar = self._emit_scalar(expression, base)[0]
            kind = scalar.rsplit(", ", 1)[1].removesuffix(");")
            element = expression + "[0]" * len(dimensions)
            count = 1
            for dimension in dimensions:
                count *= dimension
            return [f"codec_array(codec, {expression}, {count}, sizeof({element}), {kind});"]
        loops = []
        indices = []
        for dimension in dimensions:
            index = self._new_temp("index")
            loops.append((index, dimension))
            indices.append(index)
        indexed_expression = expression + "".join(f"[{index}]" for index in indices)
        base = type_node(base)
        lines = []
        if isinstance(base, c_ast.PtrDecl):
            lines = [f"if (codec->reading) {indexed_expression} = NULL;"]
        else:
            alias, resolved = self._resolve_alias(base)
            if isinstance(resolved, (c_ast.Struct, c_ast.Union)):
                lines = self._emit_compound(
                    owner_type, indexed_expression, resolved, alias)
            else:
                lines = self._emit_scalar(indexed_expression, base)
        for index, dimension in reversed(loops):
            lines = [f"for (size_t {index} = 0; {index} < {dimension}; ++{index}) {{"] + [
                "    " + line for line in lines
            ] + ["}"]
        return lines

    def generate(self):
        session = self.typedefs.get("GameSession")
        if session is None:
            raise ValueError("GameSession typedef was not found")
        alias, node = self._resolve_alias(session)
        self._add_compound(node, "GameSession")
        prototypes = [self.functions[name].splitlines()[0] + ";"
                      for name in self.function_order]
        definitions = [self.functions[name] for name in self.function_order]
        return ("/* Generated by tools/generate_game_save_codec.py. */\n\n" +
                "\n".join(prototypes) + "\n\n" +
                "\n".join(definitions))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="src/app/game_save_codec.inc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    gcc = shutil.which("gcc")
    if gcc is None:
        raise SystemExit("gcc is required to regenerate the save codec")
    with tempfile.TemporaryDirectory(prefix="rich4-save-codec-") as temp:
        stubs = Path(temp)
        (stubs / "stdint.h").write_text(STUB_STDINT, encoding="ascii")
        (stubs / "stddef.h").write_text(STUB_STDDEF, encoding="ascii")
        preprocessed = subprocess.run(
            [gcc, "-E", "-P", "-nostdinc", "-I", str(stubs), "-I",
             str(root / "src"), "-x", "c", "-"],
            input='#include "app/game_session.h"\n', text=True,
            capture_output=True, check=True,
        ).stdout
    ast = c_parser.CParser().parse(preprocessed)
    generated = CodecGenerator(ast).generate()
    output = root / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding="utf-8", newline="\n")
    print(output.relative_to(root))


if __name__ == "__main__":
    main()
