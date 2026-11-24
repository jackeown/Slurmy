"""Parse literal NAME=value assignments; never evaluate shell expressions."""
import re
import shlex

PLACEHOLDER = re.compile(r'\{\{([a-z_]+)\}\}')
PLACEHOLDERS = {'problem','cores','cpu_limit','wc_limit','mem_limit','mem_limit_mib','cpus'}

NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')


def parse(value):
    if value is None:return {}
    if not isinstance(value,str):raise ValueError('Environment variables must be text.')
    if any(ord(char)<32 and char not in '\r\n\t' or ord(char)==127 for char in value):
        raise ValueError('Environment variables cannot contain control characters.')
    result={};position=0;length=len(value)
    while position<length:
        while position<length and value[position].isspace():position+=1
        if position==length:break
        match=NAME.match(value,position)
        if not match:raise ValueError('Use environment variable names containing letters, digits and underscores, starting with a letter or underscore.')
        name=match.group();position=match.end()
        while position<length and value[position] in ' \t':position+=1
        if position==length or value[position]!='=':raise ValueError(f'Expected = after environment variable {name}.')
        if name in result:raise ValueError(f'Environment variable {name} is assigned more than once.')
        position+=1
        # Whitespace around = is allowed. A following assignment starts a new
        # variable; use quotes to include spaces or assignment-like text in a value.
        after=position
        while after<length and value[after] in ' \t':after+=1
        if after>position and re.match(r'[A-Za-z_][A-Za-z0-9_]*[ \t]*=',value[after:]):
            result[name]='';position=after;continue
        position=after;chars=[];quote=None
        while position<length:
            char=value[position]
            if quote is None and char.isspace():break
            if char==quote:
                quote=None;position+=1;continue
            if quote is None and char in "\"'":
                quote=char;position+=1;continue
            if char=='\\' and quote!="'":
                position+=1
                if position==length:raise ValueError(f'Incomplete escape in environment variable {name}.')
                char=value[position]
            chars.append(char);position+=1
        if quote:raise ValueError(f'Unclosed quote in environment variable {name}.')
        result[name]=''.join(chars)
    for name,text in result.items():
        for match in PLACEHOLDER.finditer(text):
            if match[1] not in PLACEHOLDERS:
                raise ValueError(f'Unknown environment placeholder {match[0]} in {name}.')
        remainder=PLACEHOLDER.sub('',text)
        if '{{' in remainder or '}}' in remainder:
            raise ValueError(f'Unsupported placeholder syntax in environment variable {name}. Use {{{{cores}}}}, for example.')
    return result


def exports(values, replacements=None):
    """Quote literal fragments separately from trusted shell-word replacements.

    The problem replacement is a shell expression for its cluster path. Never
    interpolate raw user text into that expression or reparse a resolved value.
    """
    lines=[]
    for name,value in values.items():
        parts=[];position=0
        for match in PLACEHOLDER.finditer(value):
            if replacements is None or match[1] not in replacements:
                raise ValueError(f'Environment placeholder {match[0]} is not available for this solver configuration.')
            parts.extend([shlex.quote(value[position:match.start()]), replacements[match[1]]])
            position=match.end()
        parts.append(shlex.quote(value[position:]))
        lines.append(f'export {name}={"".join(parts)}\n')
    return ''.join(lines)
