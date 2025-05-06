class EditorConsole {
  constructor(model, editorTabs, editor) {
    Module._set_result_callback(
      Module.addFunction(this.upload.bind(this), "viiii")
    );

    this.model = model;
    this.editor = editor;
    this.editorTabs = editorTabs;
    this.running = false;

    this.consoleStatus = document.getElementById("console-status");
    this.consoleOutput = document.getElementById("console-output");
    this.consoleButton = document.getElementById("console-action");
    this.consoleButtonText = document.getElementById("console-action-text");
    this.consoleButtonStartIcon = document.getElementById(
      "console-action-start-icon"
    );
    this.consoleButtonStopIcon = document.getElementById(
      "console-action-stop-icon"
    );

    this.consoleButton.onclick = () => this.onClick();
  }

  upload(size, data, sourceMapSize, sourceMap) {
    if (!size) {
      this.clear();
      this.error("Cannot run program due to internal compiler error.\n");
      return;
    }

    const bytecode = Module.HEAPU8.subarray(data, data + size);
    const debug = Module.HEAPU8.subarray(sourceMap, sourceMap + sourceMapSize);

    this.running = true;

    this.consoleButtonText.innerText = "Stop";
    this.consoleButtonStopIcon.style.display = "";
    this.consoleButtonStartIcon.style.display = "none";

    this.executionStartTime = performance.now();
    this.executionTime = 0;
    this.executionTimer = setInterval(() => this.onTimer(), 100);

    if (this.worker) {
      this.worker.terminate();
    }

    this.worker = new Worker("worker.js");
    this.worker.onerror = () => this.onWorkerError();
    this.worker.onmessageerror = () => this.onWorkerError();
    this.worker.onmessage = (event) => this.onWorkerMessage(event);
    this.worker.postMessage({ type: "start", bytecode, debug });

    this.clear();
    this.onTimer();

    Module._free(data);
    Module._free(sourceMap);
  }

  start() {
    if (this.editor.errors.length) {
      this.clear();

      for (const error of this.editor.errors) {
        this.print(
          this.editorTabs.getTabName() +
          ".cy:" +
          error.startLineNumber +
          ":" +
          error.startColumn +
          ": <span style='color:red'>error: </span>" +
          error.message +
          "\n"
        );
      }

      return;
    }

    if (this.running) {
      throw new Error("interpreter is already running");
    }

    Module._run(this.editor.getText(), true);
  }

  stop(terminate) {
    if (!this.running) {
      throw new Error("interpreter is already stopped");
    }

    this.running = false;

    this.consoleButtonText.innerText = "Run";
    this.consoleButtonStopIcon.style.display = "none";
    this.consoleButtonStartIcon.style.display = "";

    if (terminate) {
      this.worker.terminate();
      this.worker = undefined;
    }

    clearInterval(this.executionTimer);
    this.executionTimer = undefined;
    this.executionTime = (performance.now() - this.executionStartTime) / 1000;

    this.setStatus(
      `Press "Run" to start a program. <br><span>(Program ran for ${this.executionTime.toFixed(
        3
      )} seconds.)</span>`
    );
  }

  clear() {
    this.consoleOutput.textContent = "";
  }

  error(text) {
    this.print("<span style='color:red'>error: </span>" + text);
  }

  print(text) {
    const shouldScrollToBottom =
      this.consoleOutput.scrollTop +
      this.consoleOutput.clientHeight -
      this.consoleOutput.scrollHeight >=
      -5;

    this.consoleOutput.innerHTML += text;

    if (shouldScrollToBottom) {
      this.consoleOutput.scrollTo(0, this.consoleOutput.scrollHeight);
    }
  }

  onClick() {
    if (!this.running) {
      this.start();
    } else {
      this.stop(true);
      this.print("Program was terminated.\n");
    }
  }

  onTimer() {
    this.executionTime = (performance.now() - this.executionStartTime) / 1000;
    this.setStatus(
      `Program is running. (${this.executionTime.toFixed(1)} seconds)`
    );
  }

  onWorkerError() {
    this.stop();
    this.print("Program crashed unexpectedly due to an error:\n");
  }

  onWorkerMessage(event) {
    const data = event.data;

    switch (data.type) {
      case "stop": {
        this.stop();
        break;
      }

      case "print": {
        this.print(data.text + "\n");
        break;
      }

      case "error": {
        this.onWorkerError();
        this.print("\n");
        this.error(data.message);
        break;
      }
    }
  }

  setStatus(status) {
    this.consoleStatus.innerHTML = status;
  }
}

class EditorTabs {
  #LOCAL_STORAGE_KEY = "Cyth";

  constructor(editor) {
    this.editor = editor;

    this.tabIndex = 0;
    this.tabs = [];

    this.tabElements = document.getElementById("editor-tabs-list");
    this.tabElements.onwheel = (event) => {
      event.preventDefault();
      this.tabElements.scrollLeft += event.deltaY;
    };

    this.addTabButton = document.getElementById("editor-tabs-add-button");
    this.addTabButton.onclick = () => this.addTab();
  }

  load() {
    const json = JSON.parse(
      window.localStorage.getItem(this.#LOCAL_STORAGE_KEY)
    );

    if (!json) {
      this.tabIndex = 0;
      this.tabs.push({
        name: "fibonacci",
        text: `import "env"\n  void log(int n)\n\nint fibonacci(int n)\n  if n == 0\n    return n\n  else if n == 1\n    return n\n  else\n    return fibonacci(n - 2) + fibonacci(n - 1)\n\nfor int i = 0; i <= 42; i = i + 1\n  log(fibonacci(i))`,
      });
      this.tabs.push({
        name: "pascal",
        text: `import "env"\n  void log(int n)\n\nint binomialCoeff(int n, int k)\n  int res = 1\n\n  if k > n - k\n    k = n - k\n\n  int i\n  while i < k\n    res = res * (n - i)\n    res = res / (i + 1)\n\n    i = i + 1\n\n  return res \n\nint index\nint count = 16\n\nwhile index < count\n  log(\n    binomialCoeff(count  - 1, index)\n  )\n\n  index = index + 1`,
      });
      this.tabs.push({
        name: "sqrt",
        text: `import "env"\n  void log(float n)\n\nint sqrt(int x)\n  int s\n  int b = 32768\n\n  while b != 0\n    int t = s + b\n\n    if t * t <= x\n      s = t\n\n    b = b / 2\n\n  return s \n\nint pow(int base, int exp)\n  int result = 1\n\n  while exp != 0\n    if exp % 2 == 1\n      result = result * base\n\n    exp = exp / 2 \n    base = base * base \n\n  return result\n\nint result2 = sqrt(456420496)\nint result = pow(result2, 2)\n\nlog(result2 + 0.0)\nlog(result + 0.0)\n\nfloat sqrtf(float n)\n  float x = n\n  float y = 1.0\n  float e = 0.000001\n\n  while x - y > e\n    x = (x + y) / 2.0\n    y = n / x\n\n  return x \n\nlog(sqrtf(50.0)) \n\nfloat exponential(float n, float x)\n  float sum = 1.0\n\n  for float i = n - 1; i > 0; i = i - 1\n    sum = (x * sum / i) + 1.0\n\n  return sum\n\nfloat n = 10.0\nfloat x = 1.0\n\nlog(exponential(n, x))`,
      });
      this.tabs.push({
        name: "quick_sort",
        text: `import "env"\n    void log(int n)\n\nclass QuickSort<T>\n  void sort(T[] array)\n    int low = 0\n    int high = array.length - 1\n\n    if low >= high\n      return\n\n    int[] stack\n    stack.push(low)\n    stack.push(high)\n\n    while stack.length > 0\n      high = stack.pop()\n      low = stack.pop()\n\n      int p = partition(array, low, high)\n\n      if p - 1 > low\n        stack.push(low)\n        stack.push(p - 1)\n\n      if p + 1 < high\n        stack.push(p + 1)\n        stack.push(high)\n\n  int partition(T[] array, int low, int high)\n    T pivot = array[high]\n    int i = low - 1\n\n    for int j = low; j < high; j = j + 1\n      if array[j] <= pivot\n        i = i + 1\n\n        swap(array, i, j)\n\n    swap(array, i + 1, high)\n    return i + 1\n\n  void swap(T[] array, int i, int j)\n    T temp = array[i]\n    array[i] = array[j]\n    array[j] = temp\n\nint[] array = [\n  55, 47, 35, 15, 20, 42,\n  52, 30, 58, 15, 13, 19,\n  32, 18, 44, 11, 7, 9,\n  34, 56, 17, 25, 14, 48,\n  40, 4, 5, 7, 36, 1,\n  33, 49, 25, 26, 30, 9\n]\n\nQuickSort<int> sorter = QuickSort<int>()\nsorter.sort(array)\n\nfor int i = 0; i < array.length; i = i + 1\n  log(array[i])`,
      });
    } else {
      this.tabs = json.tabs;
      this.tabIndex = json.tabIndex;
    }

    this.selectTab(this.tabIndex);
  }

  save() {
    const json = JSON.stringify({ tabIndex: this.tabIndex, tabs: this.tabs });

    window.localStorage.setItem(this.#LOCAL_STORAGE_KEY, json);
  }

  render() {
    this.tabElements.innerHTML = "";

    let activeTab;

    for (let index = 0; index < this.tabs.length; index++) {
      const tab = this.tabs[index];

      const tabElement = document.createElement("div");
      tabElement.onclick = () => this.selectTab(index);
      tabElement.className = "editor-tab";
      tabElement.innerHTML = `
                <div class="editor-tab-icon">CY</div>
                ${tab.name}
            `;

      if (this.tabIndex == index) {
        const removeTabElement = document.createElement("div");
        removeTabElement.onclick = (event) => {
          event.stopPropagation();
          this.removeTab();
        };
        removeTabElement.className = "editor-tab-delete";
        removeTabElement.innerHTML = `
                    <svg class="editor-tab-delete" xmlns="http://www.w3.org/2000/svg" fill="#fff" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" x="0px" y="0px" 
                        viewBox="0 0 357 357" xml:space="preserve">
                        <polygon points="357,35.7 321.3,0 178.5,142.8 35.7,0 0,35.7 142.8,178.5 0,321.3 35.7,357 178.5,214.2 321.3,357 357,321.3 214.2,178.5 "/>     
                    </svg>
                `;

        tabElement.className += " editor-tab-active";
        tabElement.appendChild(removeTabElement);

        activeTab = tabElement;
      }

      this.tabElements.appendChild(tabElement);
    }

    if (activeTab) {
      activeTab.scrollIntoView();
    }
  }

  addTab() {
    const name = prompt("Please enter a name:");

    if (name) {
      this.tabs.push({ name, text: "" });

      this.save();
      this.selectTab(this.tabs.length - 1);
    }
  }

  removeTab() {
    if (
      confirm(
        `Are you sure you want to delete '${this.tabs[this.tabIndex].name}'?`
      )
    ) {
      this.tabs.splice(this.tabIndex, 1);
      this.save();

      if (this.tabIndex == this.tabs.length) {
        this.selectTab(this.tabIndex - 1);
      } else {
        this.selectTab(this.tabIndex);
      }
    }
  }

  selectTab(index) {
    if (index < 0 || index >= this.tabs.length) {
      this.tabIndex = 0;
    } else {
      this.tabIndex = index;
    }

    if (this.tabs.length) {
      this.editor.setText(this.tabs[this.tabIndex].text);
      this.editor.setReadOnly(false);
    } else {
      this.editor.setText("");
      this.editor.setReadOnly(true);
    }

    this.render();
  }

  getTabName() {
    if (this.tabIndex < 0 || this.tabIndex >= this.tabs.length) {
      return "<unkown>";
    }

    return this.tabs[this.tabIndex].name;
  }

  setText(text) {
    if (this.tabIndex < 0 || this.tabIndex >= this.tabs.length) {
      return;
    }

    this.tabs[this.tabIndex].text = text;
    this.save();
  }
}

class Editor {


  constructor() {
    require.config({
      paths: {
        vs: "https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.21.2/min/vs",
      },
    });
  }

  init() {
    Module._set_error_callback(
      Module.addFunction(this.onError.bind(this), "viiiii")
    );

    require(["vs/editor/editor.main"], () => {
      monaco.languages.register({ id: "cyth" });
      monaco.languages.setLanguageConfiguration("cyth", {
        surroundingPairs: [
          { open: "{", close: "}" },
          { open: "(", close: ")" },
          { open: "[", close: "]" },
        ],
        autoClosingPairs: [
          { open: "{", close: "}" },
          { open: "(", close: ")" },
          { open: "[", close: "]" },
        ],
        brackets: [
          ["{", "}"],
          ["(", ")"],
          ["[", "]"],
        ],
        comments: { lineComment: "#" },
      });

      monaco.languages.setMonarchTokensProvider("cyth", {
        types: [
          "import",
          "string",
          "int",
          "float",
          "bool",
          "char",
          "class",
          "void",
          "null",
          "true",
          "false",
          "this",
          "and",
          "or",
          "not",
          "inf",
          "nan",
        ],
        keywords: ["return", "for", "while", "break", "continue", "if", "else"],
        operators: [
          "=",
          ">",
          "<",
          "!",
          "~",
          "?",
          ":",
          "==",
          "<=",
          ">=",
          "!=",
          "&&",
          "||",
          "++",
          "--",
          "+",
          "-",
          "*",
          "/",
          "&",
          "|",
          "^",
          "%",
          "<<",
          ">>",
          ">>>",
          "+=",
          "-=",
          "*=",
          "/=",
          "&=",
          "|=",
          "^=",
          "%=",
          "<<=",
          ">>=",
          ">>>=",
        ],
        symbols: /[=><!~?:&|+\-*\/\^%]+/,
        tokenizer: {
          root: [
            [/#.*/, "comment"],
            [/([a-zA-Z_][a-zA-Z_0-9]*)(\()/, ["function", "default"]],
            [/\d+\.[fF]/, "number"],
            [/\d*\.\d+([eE][\-+]?\d+)?[Ff]?/, "number"],
            [/0[xX][0-9a-fA-F]+[uU]/, "number"],
            [/0[xX][0-9a-fA-F]+/, "number"],
            [/\d+[uU]/, "number"],
            [/\d+/, "number"],
            [/[A-Z][a-zA-Z_]*[\w$]*/, "class"],
            [
              /[a-zA-Z_$][\w$]*/,
              {
                cases: {
                  "@keywords": "keyword",
                  "@types": "types",
                  "@default": "identifier",
                },
              },
            ],
            [/[{}()\[\]]/, "default"],
            [/[;,.]/, "default"],
            [
              /@symbols/,
              {
                cases: {
                  "@operators": "operator",
                  "@default": "",
                },
              },
            ],
            { include: "@whitespace" },

            [/("|')([^"\\]|\\.)*$/, "string.invalid"],
            [
              /("|')/,
              { token: "string.quote", bracket: "@open", next: "@string" },
            ],

            [/("|')[^\\("|')]("|')/, "string"],
            [/("|')/, "string.invalid"],
          ],

          string: [
            [/[^\\("|')]+/, "string"],
            [/\\./, "string.escape.invalid"],
            [
              /("|')/,
              { token: "string.quote", bracket: "@close", next: "@pop" },
            ],
          ],

          whitespace: [[/[ \t\r\n]+/, "white"]],
        },
      });

      monaco.editor.defineTheme("cyth", {
        base: "vs-dark",
        inherit: true,
        rules: [
          { token: "keyword", foreground: "f7a8ff" },
          { token: "types", foreground: "00c4ff" },
          { token: "identifier", foreground: "9cdcfe" },
          { token: "number", foreground: "adfd84" },
          { token: "function", foreground: "fde3a1" },
          { token: "comment", foreground: "00af1b" },
          { token: "operator", foreground: "c0c0c0" },
          { token: "class", foreground: "92ffc7" },
          { token: "default", foreground: "909090" },
        ],
        colors: {
          "editor.background": "#1d1d1d",
          "editorLineNumber.foreground": "#858585",
          "scrollbar.shadow": "#00000000",
        },
      });

      this.errors = [];
      this.model = monaco.editor.createModel("", "cyth");
      this.model.updateOptions({ tabSize: 2 });
      this.encoder = new TextEncoder();

      this.editorDeltaDecorationsList = [];
      this.editorElement = document.getElementById("editor");
      this.editor = monaco.editor.create(this.editorElement, {
        theme: "cyth",
        lineNumbers: "on",
        model: this.model,
        automaticLayout: true,
        scrollBeyondLastLine: true,
        minimap: { enabled: false },
        fixedOverflowWidgets: true,
      });

      this.editorTabs = new EditorTabs(this);
      this.editorConsole = new EditorConsole(this.model, this.editorTabs, this);

      this.editor.layout();
      this.editor.addCommand(
        monaco.KeyMod.CtrlCmd | monaco.KeyCode.KEY_S,
        () => { }
      );
      this.editor.onDidChangeModelContent(() => this.onInput());

      this.editorTabs.load();
    });
  }

  setText(text) {
    this.editor.setValue(text);
    this.editor.setScrollPosition({ scrollTop: 0 });
  }

  setReadOnly(value) {
    this.editor.updateOptions({ readOnly: value });
  }

  getText() {
    const text = this.editor.getValue();
    const data = this.encoder.encode(text);
    const offset = Module._memory_alloc(data.byteLength + 1);
    Module.HEAPU8.set(data, offset);
    Module.HEAPU8[offset + data.byteLength] = 0;

    return offset;
  }

  onError(startLineNumber, startColumn, endLineNumber, endColumn, message) {
    this.errors.push({
      startLineNumber,
      startColumn,
      endLineNumber,
      endColumn,
      message: Module.UTF8ToString(message),
    });
  }

  onInput() {
    this.errors.length = 0;

    Module._run(this.getText(), false);

    this.editorTabs.setText(this.editor.getValue());

    monaco.editor.setModelMarkers(this.model, "owner", this.errors);
  }
}

const editor = new Editor();

var Module = {
  preRun: [],
  onRuntimeInitialized: function () {
    editor.init();
  },
};
