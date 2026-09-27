// Everything the widget editor page needs from CodeMirror, exposed as window.CM.
import { EditorState, Compartment } from "@codemirror/state";
import { EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter,
         drawSelection, dropCursor, rectangularSelection, crosshairCursor, highlightSpecialChars } from "@codemirror/view";
import { defaultKeymap, history, historyKeymap, indentWithTab, undo, redo } from "@codemirror/commands";
import { bracketMatching, foldGutter, foldKeymap, indentOnInput, syntaxHighlighting,
         HighlightStyle, syntaxTree } from "@codemirror/language";
import { closeBrackets, closeBracketsKeymap } from "@codemirror/autocomplete";
import { searchKeymap, highlightSelectionMatches, search } from "@codemirror/search";
import { linter, lintGutter, lintKeymap } from "@codemirror/lint";
import { json } from "@codemirror/lang-json";
import { tags } from "@lezer/highlight";

const theme = EditorView.theme({
  "&": { backgroundColor: "#151515", color: "#e6e6e6", fontSize: "13px", border: "1px solid #444", borderRadius: "6px" },
  ".cm-content": { fontFamily: "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace", caretColor: "#fff" },
  ".cm-gutters": { backgroundColor: "#111", color: "#666", border: "none" },
  ".cm-activeLine": { backgroundColor: "#1f1f1f" },
  ".cm-activeLineGutter": { backgroundColor: "#1f1f1f" },
  "&.cm-focused .cm-selectionBackground, .cm-selectionBackground": { backgroundColor: "#2a4a6a" },
  ".cm-selectionMatch": { backgroundColor: "#2a3a2a" },
  ".cm-matchingBracket": { backgroundColor: "#3a3a1a", outline: "1px solid #886" },
  ".cm-lintRange-error": { backgroundImage: "none", borderBottom: "2px solid #f66" },
  ".cm-tooltip": { backgroundColor: "#222", color: "#eee", border: "1px solid #444" },
  ".cm-panels": { backgroundColor: "#1a1a1a", color: "#eee" },
  ".cm-panel input, .cm-panel button": { color: "#eee", backgroundColor: "#222", border: "1px solid #444" },
  "&.cm-focused": { outline: "1px solid #3c3" }
}, { dark: true });

const highlight = HighlightStyle.define([
  { tag: tags.propertyName, color: "#8ec8ff" },
  { tag: tags.string, color: "#d8b46a" },
  { tag: tags.number, color: "#a8e08a" },
  { tag: tags.bool, color: "#f090c0" },
  { tag: tags.null, color: "#f090c0" },
  { tag: tags.punctuation, color: "#999" },
  { tag: tags.invalid, color: "#f66" }
]);

// Creates an editor in `parent`. `onChange(text)` fires on every edit;
// `lint(text)` returns [{from, to, message}] diagnostics.
function create(parent, initial, onChange, lint) {
  const lintSource = linter(view => (lint ? lint(view.state.doc.toString(), view) : []).map(d => ({ from: d.from, to: d.to, severity: d.severity || "error", message: d.message })), { delay: 250 });
  const state = EditorState.create({
    doc: initial || "",
    extensions: [
      lineNumbers(), highlightActiveLineGutter(), highlightSpecialChars(), history(), foldGutter(),
      drawSelection(), dropCursor(), EditorState.allowMultipleSelections.of(true), indentOnInput(),
      syntaxHighlighting(highlight, { fallback: true }), bracketMatching(), closeBrackets(),
      rectangularSelection(), crosshairCursor(), highlightActiveLine(), highlightSelectionMatches(),
      search({ top: true }), lintGutter(), lintSource,
      keymap.of([...closeBracketsKeymap, ...defaultKeymap, ...searchKeymap, ...historyKeymap, ...foldKeymap, ...lintKeymap, indentWithTab]),
      json(), theme, EditorState.tabSize.of(2),
      EditorView.updateListener.of(u => { if (u.docChanged && onChange) onChange(u.state.doc.toString()); })
    ]
  });
  const view = new EditorView({ state, parent });
  return {
    view,
    get: () => view.state.doc.toString(),
    set: text => view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } }),
    insert: text => { const { from, to } = view.state.selection.main; view.dispatch({ changes: { from, to, insert: text }, selection: { anchor: from + text.length } }); view.focus(); },
    focus: () => view.focus(),
    undo: () => undo(view),
    redo: () => redo(view),
    // Node names with positions, for debugging path lookup.
    nodes: (limit = 60) => {
      const out = [];
      syntaxTree(view.state).iterate({ enter: n => { if (out.length < limit) out.push(n.name + '@' + n.from + '-' + n.to); } });
      return out;
    },
    // Locates the JSON node for a path like "elements.2.children.1" or a top-level key; returns {from, to} or null.
    locate: path => {
      const tree = syntaxTree(view.state), doc = view.state.doc;
      let node = tree.topNode.firstChild;   // JsonText -> Object
      if (!node) return null;
      for (const seg of String(path).split(".")) {
        if (!node) return null;
        if (node.name === "Object") {
          let found = null;
          for (let p = node.firstChild; p; p = p.nextSibling) {
            if (p.name !== "Property") continue;
            const nameNode = p.firstChild;
            if (nameNode && doc.sliceString(nameNode.from, nameNode.to) === JSON.stringify(seg)) {
              found = nameNode.nextSibling;
              while (found && found.name === ":") found = found.nextSibling;   // the colon is a node too
              break;
            }
          }
          node = found;
        } else if (node.name === "Array") {
          const idx = parseInt(seg, 10);
          let i = 0, found = null;
          for (let c = node.firstChild; c; c = c.nextSibling) {
            if (c.name === "[" || c.name === "]" || c.name === ",") continue;
            if (i === idx) { found = c; break; }
            i++;
          }
          node = found;
        } else return null;
      }
      return node ? { from: node.from, to: node.to } : null;
    }
  };
}

window.CM = { create };
