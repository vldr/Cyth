const vscode = require("vscode");

let cythReadyResolve;
const cythReady = new Promise((resolve) => {
  cythReadyResolve = resolve;
});

const cyth = require("./cyth");
cyth["onRuntimeInitialized"] = function () {
  cythReadyResolve();
};

async function activate(context) {
  await cythReady;

  let documents = new Map();
  let uri;

  cyth._set_error_callback(
    cyth.addFunction(
      (startLineNumber, startColumn, endLineNumber, endColumn, message) => {
        const start = new vscode.Position(startLineNumber - 1, startColumn - 1);
        const end = new vscode.Position(endLineNumber - 1, endColumn - 1);

        const error = new vscode.Diagnostic(
          new vscode.Range(start, end),
          cyth.UTF8ToString(message),
          vscode.DiagnosticSeverity.Error
        );

        error.source = "cyth";
 
        documents.get(uri).errors.push(error);
      },
      "viiiii"
    )
  );

  cyth._set_link_callback(
    cyth.addFunction(
      (refLineNumber, refColumn, defLineNumber, defColumn, length) => {
        documents.get(uri).links.push({
          refLineNumber: refLineNumber,
          refColumn: refColumn,
          defLineNumber: defLineNumber,
          defColumn: defColumn,
          length,
        });
      },
      "viiiii"
    )
  );

  const encoder = new TextEncoder();
  const diagnostics = vscode.languages.createDiagnosticCollection("cyth");
  context.subscriptions.push(diagnostics);

  function encodeText(text) {
    const data = encoder.encode(text);
    const offset = cyth._memory_alloc(data.byteLength + 1);
    cyth.HEAPU8.set(data, offset);
    cyth.HEAPU8[offset + data.byteLength] = 0;

    return offset;
  }

  function validate(document) {
    if (document.languageId !== "cyth")
      return;

    if (!documents.get(document.uri))
    {
      documents.set(document.uri, { 
        errors: [], 
        links: [], 
        linkSorted: false,
      });
    }
    else 
    {
      documents.get(document.uri).errors.length = 0;
      documents.get(document.uri).links.length = 0;
      documents.get(document.uri).linkSorted = false;
    }

    uri = document.uri;

    try {
      cyth._run(encodeText(document.getText()), false);
    } catch (err) {
      console.error("Analyzer crashed:", err);
      return;
    }

    diagnostics.set(document.uri, documents.get(document.uri).errors);
  }

  function provideDefinition(document, position) {
    if (!documents.get(document.uri).linkSorted) {
      documents.get(document.uri).linkSorted = true;
      documents.get(document.uri).links.sort((a, b) => {
        if (a.refLineNumber !== b.refLineNumber)
          return a.refLineNumber - b.refLineNumber;
        return a.refColumn - b.refColumn;
      });
    }

    function findLink(links, position) {
      let low = 0;
      let high = links.length - 1;

      while (low <= high) {
        const mid = Math.floor((low + high) / 2);
        const link = links[mid];

        if (position.line + 1 < link.refLineNumber) {
          high = mid - 1;
        } else if (position.line + 1 > link.refLineNumber) {
          low = mid + 1;
        } else {
          if (position.character + 1 < link.refColumn)
            high = mid - 1;
          else if (position.character + 1 > link.refColumn + link.length)
            low = mid + 1;
          else
            return link;
        }
      }

      return null;
    }

    const link = findLink(documents.get(document.uri).links, position);
    if (link) {
      return new vscode.Location(
        document.uri,
        new vscode.Range(
          link.defLineNumber - 1,
          link.defColumn - 1,
          link.defLineNumber - 1,
          link.defColumn - 1
        )
      );
    }
  }

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument(validate),
    vscode.workspace.onDidChangeTextDocument(e => validate(e.document)),
    vscode.workspace.onDidCloseTextDocument(doc => {
      documents.delete(doc.uri);
      diagnostics.delete(doc.uri);
    })
  );

  vscode.workspace.textDocuments.forEach(validate);

  vscode.languages.registerDefinitionProvider("cyth", { provideDefinition });
  vscode.languages.setLanguageConfiguration("cyth", {
    comments: { lineComment: "#" },
    brackets: [["{", "}"], ["(", ")"], ["[", "]"]],
    autoClosingPairs: [
      { open: "{", close: "}" },
      { open: "(", close: ")" },
      { open: "[", close: "]" },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    surroundingPairs: [
      { open: "{", close: "}" },
      { open: "(", close: ")" },
      { open: "[", close: "]" },
      { open: '"', close: '"' },
      { open: "'", close: "'" },
    ],
    onEnterRules: [
      {
        beforeText: /^\s*(return|break|continue)\b.*$/,
        action: { indentAction: vscode.IndentAction.None },
      },
      {
        beforeText: /^\s*[a-zA-Z_][a-zA-Z_0-9]*\s+[a-zA-Z_][a-zA-Z_0-9]*\(.*\)\s*$/,
        action: { indentAction: vscode.IndentAction.Indent },
      },
      {
        beforeText: /^\s*(class|for|while|if|import)\s+.*$/,
        action: { indentAction: vscode.IndentAction.Indent },
      },
      {
        beforeText: /^\s*(else\s*|else\s+if(\s+|\().*)$/,
        action: { indentAction: vscode.IndentAction.Indent },
      },
    ],
  });
}

exports.activate = activate;