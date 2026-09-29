'use strict';
// Token categories follow the TPTP grammar in teaching/vampireGuide's
// prism-tptp.js. Build DOM nodes rather than interpreting problem text as HTML.
const tptpKeywords = new Set(['tpi', 'thf', 'tff', 'tcf', 'fof', 'cnf', 'include']);
const tptpRoles = new Set(['axiom', 'hypothesis', 'definition', 'assumption', 'lemma',
  'theorem', 'corollary', 'conjecture', 'negated_conjecture', 'plain', 'type',
  'interpretation', 'fi_domain', 'fi_functors', 'fi_predicates', 'unknown']);
const tptpTokens = /%[^\r\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`[A-Z][A-Za-z0-9_]*`|\$\$?[A-Za-z0-9_]+|\b\d+\/[1-9]\d*\b|\b\d+(?:\.\d+)?(?:[Ee][+-]?\d+)?\b|\b[A-Za-z][A-Za-z0-9_]*\b|<=>|<~>|-->|=>|<=|~\||~&|!>|\?\*|@@[+-]|@[=+-]|!=|==|:=|[!?^=&|~*+<>@:]|[()[\],.]/g;

function tptpKind(token, following) {
  if (token.startsWith('%') || token.startsWith('/*')) return 'comment';
  if (token.startsWith('"')) return 'string';
  if (token.startsWith("'") || token.startsWith('`')) return 'atom';
  if (token.startsWith('$')) return token === '$true' || token === '$false' ? 'boolean' : 'builtin';
  if (/^\d/.test(token)) return 'number';
  if (/^[A-Za-z]/.test(token)) {
    if (tptpKeywords.has(token) || tptpRoles.has(token)) return 'keyword';
    if (/^[A-Z]/.test(token)) return 'variable';
    return /^\s*\(/.test(following) ? 'function' : 'atom';
  }
  return /[()[\],.]/.test(token[0]) ? 'punctuation' : 'operator';
}

function highlightTptp(target, source) {
  const fragment = document.createDocumentFragment();
  let position = 0;
  tptpTokens.lastIndex = 0;
  for (const match of source.matchAll(tptpTokens)) {
    if (match.index > position) fragment.append(document.createTextNode(source.slice(position, match.index)));
    const token = document.createElement('span');
    token.className = 'tptp-' + tptpKind(match[0], source.slice(match.index + match[0].length, match.index + match[0].length + 8));
    token.textContent = match[0];
    fragment.append(token);
    position = match.index + match[0].length;
  }
  if (position < source.length) fragment.append(document.createTextNode(source.slice(position)));
  target.replaceChildren(fragment);
}
window.highlightTptp = highlightTptp;
