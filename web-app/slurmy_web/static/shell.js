'use strict';
// Small, dependency-free highlighter for displayed shell commands and editable recipes.
// Tokens are always inserted as text nodes: workflow commands are untrusted input.
function highlightShell(node, source) {
 node.replaceChildren();
 const token = /\{\{[a-z_]+\}\}|\$\{[A-Za-z_][\w]*\}|\$[A-Za-z_][\w]*|"(?:\\.|[^"\\])*"|'[^']*'|(?:^|\s)#[^\n]*|--?[A-Za-z][\w-]*(?:=[^\s]+)?|(?:^|\s)(?:\/[^\s'";|]+|\.\/?[^\s'";|]+)|[|;&<>]+/gm;
 let at=0,match;
 while((match=token.exec(source))){
  if(match.index>at)node.append(document.createTextNode(source.slice(at,match.index)));
  const value=match[0],lead=/^\s/.exec(value)?.[0]||'';
  if(lead)node.append(document.createTextNode(lead));
  const word=value.slice(lead.length),span=document.createElement('span');span.textContent=word;
  span.className=word.startsWith('{{')?'sh-placeholder':word.startsWith('$')?'sh-variable':word.startsWith('#')?'sh-comment':word.startsWith('-')?'sh-option':word.startsWith('"')||word.startsWith("'")?'sh-string':word.startsWith('/')||word.startsWith('.')?'sh-path':'sh-operator';
  node.append(span);at=token.lastIndex;
 }
 if(at<source.length)node.append(document.createTextNode(source.slice(at)));
}
function shellEditor(textarea){
 if(textarea.closest('.shell-editor'))return;
 const wrapper=document.createElement('div'),overlay=document.createElement('pre');wrapper.className='shell-editor';overlay.className='shell-overlay';overlay.setAttribute('aria-hidden','true');
 textarea.replaceWith(wrapper);wrapper.append(overlay,textarea);
 const refresh=()=>{highlightShell(overlay,textarea.value||textarea.placeholder);overlay.append(document.createTextNode('\n'));overlay.scrollTop=textarea.scrollTop;overlay.scrollLeft=textarea.scrollLeft;};
 textarea.addEventListener('input',refresh);textarea.addEventListener('scroll',()=>{overlay.scrollTop=textarea.scrollTop;overlay.scrollLeft=textarea.scrollLeft;});refresh();
 textarea.refreshShell=refresh;
}
function shellEditors(root=document){root.querySelectorAll('textarea[name="command"],textarea[name="limiter"],textarea[name="commands"]').forEach(shellEditor);}
document.addEventListener('DOMContentLoaded',()=>{
 shellEditors();
 document.querySelectorAll('pre[data-shell]').forEach(node=>highlightShell(node,node.textContent));
});
