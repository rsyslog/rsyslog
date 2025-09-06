# source/_ext/rsyslog_lexer.py

import re

from pygments.lexer import RegexLexer, bygroups
from pygments.token import (
    Text, Comment, Operator, Keyword, Name, String, Number, Punctuation,
    Error
)

__all__ = ['RainerScriptLexer']


class RainerScriptLexer(RegexLexer):
    """
    Pygments lexer for Rsyslog's RainerScript.
    """
    name = 'RainerScript'
    aliases = ['rsyslog', 'rainerscript']
    filenames = ['*.conf', '*.rs', '*.rainerscript']
    mimetypes = ['text/x-rainerscript']

    # Keywords from the lexer file
    keywords = [
        'if', 'then', 'else', 'call', 'call_indirect', 'set', 'unset',
        'reset', 'stop', 'continue', 'foreach', 'do', 'return'
    ]

    # Object definition keywords
    objects = [
        'action', 'input', 'module', 'template', 'ruleset', 'parser',
        'global', 'property', 'constant', 'lookup_table', 'main_queue',
        'dyn_stats', 'percentile_stats', 'timezone', 'include',
        'reload_lookup_table'
    ]
    
    # Built-in functions and system properties
    builtins = [
        'exec_template', 'lookup', 're_match', 're_extract', 'field', 'dyn_inc',
        'http_request', 'getenv', 'uuid', 'random', 'get_metadata',
        'set_metadata', 'unset_metadata', 'exists', 'int', 'string',
        'get_var', 'hostname', 'ip', 'programname', 'msg', 'rawmsg',
        'syslogtag', 'protocol-version', 'structured-data', 'app-name',
        'procid', 'msgid', 'pri', 'pri-text', 'syslogfacility',
        'syslogfacility-text', 'syslogseverity', 'syslogseverity-text',
        'fromhost', 'fromhost-ip', 'fromhost-port', 'remotehost', 'remotehost-ip', 'timereported',
        'timegenerated', 'timestamp', 'json', 'jsonmesg', 'jsonf'
    ]

    tokens = {
        'root': [
            (r'\s+', Text),
            (r'#.*?\n', Comment.Single),
            (r'/\*', Comment.Multiline, 'comment'),
            
            # Legacy syslog selectors at the start of a line
            (r'^\s*([a-zA-Z0-9\.\*]+;)?([a-zA-Z0-9\.\*]+)\.(=|!=|=,!=)?([a-zA-Z0-9\*]+)\s+',
             bygroups(Keyword.Namespace, Keyword.Namespace, Operator, Keyword.Namespace), 'legacy_action'),
            (r'^\s*:([a-zA-Z0-9_-]+),\s*(==|!=|<>|contains|startswith|endswith|re_match|re_extract)\s*"[^"]*"\s+',
             bygroups(Name.Tag, Operator.Word, String.Double), 'legacy_action'),
            
            # RainerScript Keywords
            (r'\b(%s)\b' % '|'.join(keywords), Keyword.Reserved),
            (r'\b(%s)\b' % '|'.join(objects), Keyword.Declaration),
            (r'\b(on|off|yes|no|true|false)\b', Keyword.Constant),
            
            # Operators
            (r'(&&|\|\||and|or|not)\b', Operator.Word),
            (r'(=|==|!=|<>|<=|>=|<|>|:=|contains_i|startswith_i|contains|startswith|endswith)', Operator),

            # Variables and Properties
            (r'\$[a-zA-Z0-9_.-]+', Name.Variable),
            (r'\$\![\w\.-]+', Name.Variable.Instance),
            (r'\$\.[\w\.-]+', Name.Variable.Global),

            # Built-in functions
            (r'\b(%s)\b' % '|'.join(builtins), Name.Builtin),

            # Strings
            (r'"', String.Double, 'double_string'),
            (r"'", String.Single, 'single_string'),
            (r'`', String.Backtick, 'backtick_string'),

            # Numbers
            (r'\b(0x[0-9a-fA-F]+)\b', Number.Hex),
            (r'\b\d+\b', Number.Integer),
            
            # Punctuation
            (r'[{}(),;\[\].&]', Punctuation),
            
            # Identifiers
            (r'[a-zA-Z_][a-zA-Z0-9_-]*', Name), # <<<--- THIS IS THE CHANGE: Added '-' to allowed identifier characters
            
            # Legacy Directives
            (r'^\$\w+', Keyword.Pseudo),
        ],
        'comment': [
            (r'[^*/]+', Comment.Multiline),
            (r'/\*', Comment.Multiline, '#push'),
            (r'\*/', Comment.Multiline, '#pop'),
            (r'[*/]', Comment.Multiline),
        ],
        'double_string': [
            (r'\\"', String.Escape),
            (r'[^"]+', String.Double),
            (r'"', String.Double, '#pop'),
        ],
        'single_string': [
            (r"\\'", String.Escape),
            (r"[^']+", String.Single),
            (r"'", String.Single, '#pop'),
        ],
        'backtick_string': [
            (r"\\`", String.Escape),
            (r"[^`]+", String.Backtick),
            (r"`", String.Backtick, '#pop'),
        ],
        'legacy_action': [
            (r'~', Operator, '#pop'),
            (r'-?/[^;\s]+', Name.Constant, '#pop'),
            (r'\S+', Name.Constant),
            (r'.*?\n', Text, '#pop'),
        ],
    }

# --- Sphinx Extension setup function ---
def setup(app):
    from sphinx.highlighting import PygmentsBridge
    
    app.add_lexer('rainerscript', RainerScriptLexer)
    app.add_lexer('rsyslog', RainerScriptLexer) 

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
