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

  const documents = new Map();
  const encoder = new TextEncoder();
  const diagnostics = vscode.languages.createDiagnosticCollection("cyth");
  context.subscriptions.push(diagnostics);

  cyth._cyth_wasm_set_error_callback(
    cyth.addFunction(
      (filename, startLineNumber, startColumn, endLineNumber, endColumn, message) => {
        const document = documents.get(cyth.UTF8ToString(filename));
        if (!document)
          return;

        const start = new vscode.Position(startLineNumber - 1, startColumn - 1);
        const end = new vscode.Position(endLineNumber - 1, endColumn - 1);
        const error = new vscode.Diagnostic(
          new vscode.Range(start, end),
          cyth.UTF8ToString(message),
          vscode.DiagnosticSeverity.Error
        );

        document.errors.push(error);
      },
      "viiiiii"
    )
  );

  cyth._cyth_wasm_set_link_callback(
    cyth.addFunction(
      (refFilename, refLineNumber, refColumn, defFilename, defLineNumber, defColumn, length) => {
        const document = documents.get(cyth.UTF8ToString(refFilename))
        if (!document)
          return;

        document.links.push({
          refLineNumber: refLineNumber,
          refColumn: refColumn,
          defFilename: cyth.UTF8ToString(defFilename),
          defLineNumber: defLineNumber,
          defColumn: defColumn,
          length,
        });
      },
      "viiiiiii"
    )
  );

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

    const uri = document.uri.toString();

    if (!documents.get(uri)) {
      documents.set(uri, {
        errors: [],
        links: [],
        linkSorted: false,
      });
    }
    else {
      documents.get(uri).errors.length = 0;
      documents.get(uri).links.length = 0;
      documents.get(uri).linkSorted = false;
    }

    const env = encodeText("env");
    const imports = vscode.workspace.getConfiguration("cyth").get("imports");
    const files = vscode.workspace.getConfiguration("cyth").get("files");

    try {
      cyth._cyth_wasm_init();

      for (const item of imports) {
        if (!cyth._cyth_wasm_load_function(encodeText(item), env))
          throw new Error(`Failed to compile function: ${item}`);
      }

      for (const filename in files) {
        const filenameUri = "cyth://cyth/" + filename;
        if (filenameUri == uri)
          continue;

        if (!cyth._cyth_wasm_load_string(encodeText(filenameUri), encodeText(files[filename])))
          throw new Error(`Failed to compile: ${filename}`);
      }

      if (cyth._cyth_wasm_load_string(encodeText(uri), encodeText(document.getText())))
        cyth._cyth_wasm_compile(false, false);

    } catch (err) {
      vscode.window.showErrorMessage(`Cyth crashed: ${err}`);
      return;
    }

    diagnostics.set(document.uri, documents.get(uri).errors);
  }

  function provideDefinition(document, position) {
    const uri = document.uri.toString();

    if (!documents.get(uri).linkSorted) {
      documents.get(uri).linkSorted = true;
      documents.get(uri).links.sort((a, b) => {
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

    const link = findLink(documents.get(uri).links, position);
    if (link) {
      return new vscode.Location(
        vscode.Uri.parse(link.defFilename),
        new vscode.Range(
          link.defLineNumber - 1,
          link.defColumn - 1,
          link.defLineNumber - 1,
          link.defColumn - 1
        )
      );
    }
  }

  function provideTextDocumentContent(uri) {
    const files = vscode.workspace.getConfiguration("cyth").get("files");
    const file = uri.toString().replace("cyth://cyth/", "");

    return files[file];
  }

  context.subscriptions.push(
    vscode.workspace.onDidOpenTextDocument(validate),
    vscode.workspace.onDidChangeTextDocument(e => validate(e.document)),
    vscode.workspace.onDidChangeConfiguration(e => {
      if (e.affectsConfiguration('cyth'))
        vscode.workspace.textDocuments.forEach(validate);
    }),
    vscode.workspace.onDidCloseTextDocument(doc => {
      documents.delete(doc.uri.toString());
      diagnostics.delete(doc.uri);
    }),
  );

  vscode.workspace.textDocuments.forEach(validate);
  vscode.workspace.registerTextDocumentContentProvider("cyth", { provideTextDocumentContent });

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