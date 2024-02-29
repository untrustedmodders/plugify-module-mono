#!/usr/bin/python3
import sys
import argparse
import os
import json
from enum import Enum

TYPES_MAP = {
    'void': 'void',
    'bool': 'bool',
    'char8': 'char',
    'char16': 'short',
    'int8': 'sbyte',
    'int16': 'short',
    'int32': 'int',
    'int64': 'long',
    'uint8': 'byte',
    'uint16': 'ushort',
    'uint32': 'uint',
    'uint64': 'ulong',
    'ptr64': 'IntPtr',
    'float': 'float',
    'double': 'double',
    'function': 'delegate',
    'string': 'string',
    'bool*': 'bool[]',
    'char8*': 'char[]',
    'char16*': 'short[]',
    'int8*': 'sbyte[]',
    'int16*': 'short[]',
    'int32*': 'int[]',
    'int64*': 'long[]',
    'uint8*': 'byte[]',
    'uint16*': 'ushort[]',
    'uint32*': 'uint[]',
    'uint64*': 'ulong[]',
    'ptr64*': 'IntPtr[]',
    'float*': 'float[]',
    'double*': 'double[]',
    'string*': 'string[]'
}

INVALID_NAMES = {
    "abstract",
    "as",
    "base",
    "bool",
    "break",
    "byte",
    "case",
    "catch",
    "char",
    "checked",
    "class",
    "const",
    "continue",
    "decimal",
    "default",
    "delegate",
    "do",
    "double",
    "else",
    "enum",
    "event",
    "explicit",
    "extern",
    "false",
    "finally",
    "fixed",
    "float",
    "for",
    "foreach",
    "goto",
    "if",
    "implicit",
    "in",
    "int",
    "interface",
    "internal",
    "is",
    "lock",
    "long",
    "namespace",
    "new",
    "null",
    "object",
    "operator",
    "out",
    "override",
    "params",
    "private",
    "protected",
    "public",
    "readonly",
    "ref",
    "return",
    "sbyte",
    "sealed",
    "short",
    "sizeof",
    "stackalloc",
    "static",
    "string",
    "struct",
    "switch",
    "this",
    "throw",
    "true",
    "try",
    "typeof",
    "uint",
    "ulong",
    "unchecked",
    "unsafe",
    "ushort",
    "using",
    "virtual",
    "void",
    "volatile",
    "while"
    #"add",
    #"and",
    #"alias",
    #"ascending",
    #"args",
    #"async",
    #"await",
    #"by",
    #"descending",
    #"dynamic",
    #"equals",
    #"file",
    #"from",
    #"get",
    #"global",
    #"group",
    #"init",
    #"into",
    #"join",
    #"let",
    #"managed",
    #"nameof",
    #"nint",
    #"not",
    #"notnull",
    #"nuint",
    #"on",
    #"or",
    #"orderby",
    #"partial",
    #"partial",
    #"record",
    #"remove",
    #"required",
    #"scoped",
    #"select",
    #"set",
    #"unmanaged",
    #"value",
    #"var",
    #"when",
    #"where",
    #"where",
    #"with",
    #"yield"
}

def validate_manifest(pplugin):
    parse_errors = []
    methods = pplugin.get('exportedMethods')
    if type(methods) is list:
        for i, method in enumerate(methods):
            if type(method) is dict:
                if type(method.get('type')) is str:
                    parse_errors += [f'root.exportedMethods[{i}].type not string']
            else:
                parse_errors += [f'root.exportedMethods[{i}] not object']
    else:
        parse_errors += ['root.exportedMethods not array']
    return parse_errors


def convert_type(type_name, is_ref=False):
    if is_ref:
        return 'ref ' + TYPES_MAP.get(type_name, 'int')
    else:
        return TYPES_MAP.get(type_name, 'int')


def generate_name(name):
    if name in INVALID_NAMES:
        return name + '_'
    else:
        return name


class ParamGen(Enum):
    Types = 1
    Names = 2
    TypesNames = 3


def gen_params_string(params, param_gen: ParamGen):
    def gen_param(param):
        if param_gen == ParamGen.Types:
            type = convert_type(param['type'], 'ref' in param and param['ref'] is True)
            if 'delegate' in type and 'prototype' in param:
                type = generate_name(param['prototype']['name'])
            return type
        if param_gen == ParamGen.Names:
            return generate_name(param['name'])
        type = convert_type(param['type'], 'ref' in param and param['ref'] is True)
        if 'delegate' in type and 'prototype' in param:
            type = generate_name(param['prototype']['name'])
        return f'{type} {generate_name(param["name"])}'

    output_string = ''
    if params:
        it = iter(params)
        output_string += gen_param(next(it))
        for p in it:
            output_string += f', {gen_param(p)}'
    return output_string


def gen_delegate(prototype):
    ret_type = prototype['retType']
    return_type = convert_type(ret_type['type'], 'ref' in ret_type and ret_type['ref'] is True)
    return (f'\tdelegate {return_type} '
            f'{prototype["name"]}({gen_params_string(prototype["paramTypes"], ParamGen.TypesNames)});\n')


def main(manifest_path, output_dir, override):
    if not os.path.isfile(manifest_path):
        print(f'Manifest file not exists {manifest_path}')
        return 1
    if not os.path.isdir(output_dir):
        print(f'Output folder not exists {output_dir}')
        return 1

    plugin_name = os.path.splitext(os.path.basename(manifest_path))[0]
    header_dir = os.path.join(output_dir, 'pps')
    if not os.path.exists(header_dir):
        os.makedirs(header_dir, exist_ok=True)
    header_file = os.path.join(header_dir, f'{plugin_name}.cs')
    if os.path.isfile(header_file) and not override:
        print(f'Already exists {header_file}')
        return 1

    with open(manifest_path, 'r', encoding='utf-8') as fd:
        pplugin = json.load(fd)

    parse_errors = validate_manifest(pplugin)
    if parse_errors:
        print('Parse fail:')
        for error in parse_errors:
            print(f'  {error}')
        return 1

    content = ''

    content += 'using System;\n'
    content += 'using System.Runtime.CompilerServices;\n'
    content += 'using System.Runtime.InteropServices;\n'
    content += '\n'
    content += f'namespace {plugin_name}\n{{'
    content += '\n'
    for method in pplugin['exportedMethods']:
        ret_type = method['retType']
        if "prototype" in ret_type:
            content += gen_delegate(ret_type['prototype'])
        for attribute in method['paramTypes']:
            if "prototype" in attribute:
                content += gen_delegate(attribute['prototype'])

    content += f'\n\tpublic static class {plugin_name}\n\t{{'
    content += '\n'
    for method in pplugin['exportedMethods']:
        content += "\t\t[MethodImplAttribute(MethodImplOptions.InternalCall)]\n"
        ret_type = method['retType']
        return_type = convert_type(ret_type['type'], 'ref' in ret_type and ret_type['ref'] is True)
        content += (f'\t\tinternal static extern {return_type} '
                    f'{method["name"]}({gen_params_string(method["paramTypes"], ParamGen.TypesNames)});\n')
    content += '\t}\n'
    content += '}\n'

    with open(header_file, 'w', encoding='utf-8') as fd:
        fd.write(content)

    return 0


def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('manifest')
    parser.add_argument('output')
    parser.add_argument('--override')
    return parser.parse_args()


if __name__ == '__main__':
    args = get_args()
    sys.exit(main(args.manifest, args.output, args.override))
