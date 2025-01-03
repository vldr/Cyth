class EditorConsole
{
    constructor(model, editorTabs, editor) 
    {
        window.set_result_callback(
            Module.addFunction(this.upload.bind(this), 'vii')
        );

        this.model = model;
        this.editor = editor;
        this.editorTabs = editorTabs;
        this.running = false;
        this.bytecode = "";

        this.consoleStatus = document.getElementById("console-status");
        this.consoleOutput = document.getElementById("console-output"); 
        this.consoleButton = document.getElementById("console-action");
        this.consoleButtonText = document.getElementById("console-action-text");
        this.consoleButtonStartIcon = document.getElementById("console-action-start-icon");
        this.consoleButtonStopIcon = document.getElementById("console-action-stop-icon");

        this.consoleButton.onclick = () => this.onClick();
    }

    upload(size, data)
    {
        if (!size)
        {
            this.clear();
            this.error("Cannot run program due to internal compiler error.\n");
            return;
        }

        const bytecode = Module.HEAPU8.subarray(data, data + size);

        this.running = true;

        this.consoleButtonText.innerText = "Stop";
        this.consoleButtonStopIcon.style.display = "";
        this.consoleButtonStartIcon.style.display = "none";

        this.executionStartTime = performance.now();
        this.executionTime = 0;
        this.executionTimer = setInterval(() => this.onTimer(), 100);

        this.worker = new Worker("worker.js");
        this.worker.onerror = () => this.onWorkerError();
        this.worker.onmessageerror = () => this.onWorkerError();
        this.worker.onmessage = (event) => this.onWorkerMessage(event);
        this.worker.postMessage({ type: "start", bytecode });

        this.clear();
        this.onTimer();

        Module._free(data);
    }

    start()
    {
        if (this.editor.errors.length)
        {
            this.clear();
   
            for (const error of this.editor.errors)
            {
                this.print(this.editorTabs.getTabName() + ".cy:" + error.startLineNumber + ":" + error.startColumn + ": <span style='color:red'>error: </span>" + error.message + "\n");
            }
            
            return;
        }

        if (this.running)
        {
            throw new Error("interpreter is already running");
        }

        if (this.worker)
        {
            throw new Error("worker is not undefined");
        }

        window.run(this.bytecode, true);
    }

    stop()
    {
        if (!this.running) 
        { 
            throw new Error("interpreter is already stopped"); 
        }

        if (!this.worker)
        {
            throw new Error("worker is undefined");
        }

        this.running = false;

        this.consoleButtonText.innerText = "Run";
        this.consoleButtonStopIcon.style.display = "none";
        this.consoleButtonStartIcon.style.display = "";
        
        this.worker.terminate();
        this.worker = undefined;

        clearInterval(this.executionTimer);
        this.executionTimer = undefined;
        this.executionTime = (performance.now() - this.executionStartTime) / 1000;
    
        this.setStatus(`Press "Run" to start a program. <br><span>(Program ran for ${this.executionTime.toFixed(3)} seconds.)</span>`);
    }

    clear()
    {
        this.consoleOutput.textContent = "";
    }

    error(text)
    {
        this.print("<span style='color:red'>error: </span>" + text);
    }

    print(text)
    {
        const shouldScrollToBottom = (
            this.consoleOutput.scrollTop +
            this.consoleOutput.clientHeight -
            this.consoleOutput.scrollHeight
        ) >= -5;

        this.consoleOutput.innerHTML += text;

        if (shouldScrollToBottom)
        {
            this.consoleOutput.scrollTo(0, this.consoleOutput.scrollHeight);
        }
    }

    onClick()
    {
        if (!this.running)
        {
            this.start();
        }
        else
        {
            this.stop();
            this.print("Program was terminated.\n");
        }
    } 

    onTimer() 
    {
        this.executionTime = (performance.now() - this.executionStartTime) / 1000;
        this.setStatus(`Program is running. (${this.executionTime.toFixed(1)} seconds)`);
    }

    onWorkerError()
    {
        this.stop();
        this.print("Program crashed unexpectedly due to an error:\n");
    }

    onWorkerMessage(event)
    {
        const data = event.data;

        switch (data.type)
        {
            case "stop": 
            {
                this.stop();
                break;
            }

            case "print": 
            {
                this.print(data.text + "\n");
                break;
            }

            case "error":
            {
                this.onWorkerError();
                this.print("\n");
                this.error(data.message);
                break;
            }
        }
    }

    setStatus(status)
    {
        this.consoleStatus.innerHTML = status;
    }

    setBytecode(bytecode)
    {
        this.bytecode = bytecode;
    }
}

class EditorTabs
{
    #LOCAL_STORAGE_KEY = "Cyth"

    constructor(editor)
    {
        this.editor = editor;

        this.tabIndex = 0;
        this.tabs = [];

        this.tabElements = document.getElementById("editor-tabs-list");
        this.tabElements.onwheel = (event) => { event.preventDefault(); this.tabElements.scrollLeft += event.deltaY; };

        this.addTabButton = document.getElementById("editor-tabs-add-button");
        this.addTabButton.onclick = () => this.addTab();
    }

    load()
    {
        const json = JSON.parse(window.localStorage.getItem(this.#LOCAL_STORAGE_KEY));

        if (!json)
        {
            this.tabIndex = 0;
            this.tabs.push({name: "fibonacci", text: `import "env"\n    void log(int n)\n\nint fibonacci(int n)\n    if n == 0 \n        return n \n    else if n == 1 \n        return n \n    else \n        return fibonacci(n - 2) + fibonacci(n - 1)\n\nfor int i = 0; i <= 42; i = i + 1\n    log(fibonacci(i))` });
            this.tabs.push({name: "pascal", text: `import "env"\n    void log(int n) \n \nint binomialCoeff(int n, int k)  \n    int res = 1 \n \n    if k > n - k  \n        k = n - k \n \n    int i \n    while i < k \n        res = res * (n - i) \n        res = res / (i + 1) \n \n        i = i + 1 \n       \n    return res \n \nint index \nint count = 16 \n \nwhile index < count \n    log( \n        binomialCoeff(count  - 1, index) \n    ) \n \n    index = index + 1` });
            this.tabs.push({name: "sqrt", text: `import "env"\n    void log(float n) \n \nint sqrt(int x) \n    int s \n    int b = 32768 \n \n    while b != 0 \n        int t = s + b  \n \n        if t * t <= x \n            s = t \n \n        b = b / 2 \n \n    return s \n \nint pow(int base, int exp) \n    int result = 1 \n \n    while exp != 0 \n        if exp % 2 == 1 \n            result = result * base \n             \n        exp = exp / 2 \n        base = base * base \n     \n    return result \n \nint result2 = sqrt(456420496) \nint result = pow(result2, 2) \n \nlog(result2 + 0.0) \nlog(result + 0.0) \n \nfloat sqrtf(float n) \n    float x = n \n    float y = 1.0 \n    float e = 0.000001 \n \n    while x - y > e  \n        x = (x + y) / 2.0 \n        y = n / x \n \n    return x \n \nlog(sqrtf(50.0)) \n \nfloat exponential(float n, float x) \n    float sum = 1.0 \n   \n    for float i = n - 1; i > 0; i = i - 1 \n        sum = (x * sum / i) + 1.0   \n   \n    return sum   \n  \nfloat n = 10.0 \nfloat x = 1.0 \n \nlog(exponential(n, x))` });
            this.tabs.push({name: "linked_list", text: `import "env"\n    void log(int n)\n\nclass Node\n    int data\n    Node next\n\n    void __init__(int data)\n        this.data = data\n\nclass LinkedList\n    Node head\n    int size\n\n    void append(int data)\n        Node node = Node(data)\n\n        if head == null\n            head = node\n        else\n            Node current = head\n            while current.next != null\n                current = current.next\n\n            current.next = node\n\n        size = size + 1\n\n    void prepend(int data)\n        Node node = Node(data)\n\n        if head == null\n            head = node\n        else\n            node.next = head\n            head = node\n\n        size = size + 1\n\n    bool pop()\n        return del(size - 1)\n\n    bool popfront()\n        return del(0)\n\n    bool del(int index)\n        bool found\n        int currentIndex\n        Node previous\n        Node current = head\n\n        while current != null\n            if index == currentIndex\n                found = true\n                break\n\n            previous = current\n            current = current.next\n            currentIndex = currentIndex + 1\n\n        if found\n            if previous == null\n                head = current.next\n            else\n                previous.next = current.next\n\n            size = size - 1\n            \n        return found\n\n    void print()\n        for Node node = head; node != null; node = node.next\n            log(node.data)\n            \nint items = 100000\nLinkedList list = LinkedList()\n\nfor int i = 0; i < items; i = i + 1\n    list.prepend(i)\n\nfor i = 0; i < items; i = i + 1\n    list.pop()\n    \nlist.print()` });
        }
        else
        {
            this.tabs = json.tabs;
            this.tabIndex = json.tabIndex;
        }

        this.selectTab(this.tabIndex);
    }
    
    save()
    {
        const json = JSON.stringify({ tabIndex: this.tabIndex, tabs: this.tabs });

        window.localStorage.setItem(this.#LOCAL_STORAGE_KEY, json);
    }

    render()
    {
        this.tabElements.innerHTML = "";

        let activeTab;

        for (let index = 0; index < this.tabs.length; index++)
        {
            const tab = this.tabs[index];

            const tabElement = document.createElement("div");
            tabElement.onclick = () => this.selectTab(index);
            tabElement.className = "editor-tab";
            tabElement.innerHTML = `
                <div class="editor-tab-icon">CY</div>
                ${tab.name}
            `;

            if (this.tabIndex == index)
            {
                const removeTabElement = document.createElement("div");
                removeTabElement.onclick = (event) => { event.stopPropagation(); this.removeTab(); };
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

        if (activeTab)
        {
            activeTab.scrollIntoView();
        }
    }

    addTab()
    {
        const name = prompt("Please enter a name:");

        if (name) 
        {
            this.tabs.push({ name, text: "" });

            this.save();
            this.selectTab(this.tabs.length - 1);
        }
    }

    removeTab()
    {
        if (confirm(`Are you sure you want to delete '${this.tabs[this.tabIndex].name}'?`))
        {
            this.tabs.splice(this.tabIndex, 1);
            this.save();

            if (this.tabIndex == this.tabs.length)
            {
                this.selectTab(this.tabIndex - 1);
            }
            else
            {
                this.selectTab(this.tabIndex);
            }
            
        }       
    }

    selectTab(index)
    {
        if (index < 0 || index >= this.tabs.length)
        {
            this.tabIndex = 0;
        }
        else
        {
            this.tabIndex = index;
        }

        if (this.tabs.length)
        {
            this.editor.setText(this.tabs[this.tabIndex].text);
            this.editor.setReadOnly(false);
        }
        else
        {
            this.editor.setText("");
            this.editor.setReadOnly(true);
        }
  
        this.render();
    }

    getTabName()
    {
        if (this.tabIndex < 0 || this.tabIndex >= this.tabs.length)
        {
            return "<unkown>";
        }

        return this.tabs[this.tabIndex].name;
    }

    setText(text)
    {
        if (this.tabIndex < 0 || this.tabIndex >= this.tabs.length)
        {
            return;
        }

        this.tabs[this.tabIndex].text = text;
        this.save();
    }
}

class Editor
{
    constructor() 
    {
        window.set_error_callback(
            Module.addFunction(this.onError.bind(this), 'viiiii')
        );

        require.config({ paths: { "vs": "https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.21.2/min/vs" } });
        require(["vs/editor/editor.main"], () => {
            monaco.languages.register({ id: "cyth" });
            monaco.languages.setLanguageConfiguration("cyth",
                {
                    surroundingPairs: [ {"open":"{","close":"}"}, {"open":"(","close":")"}, {"open":"[","close":"]"} ],
                    autoClosingPairs: [ {"open":"{","close":"}"}, {"open":"(","close":")"}, {"open":"[","close":"]"} ],
                    brackets: [ ["{","}"], ["(",")"], ["[","]"] ],
                    comments: { lineComment: "#" },
                }
            );
            
            monaco.languages.setMonarchTokensProvider("cyth", {
                types: [
                    'import',
                    'string',
                    'int',
                    'float',
                    'bool',
                    'class',
                    'void', 
                    'null',
                    'true',
                    'false',
                    'this',
                    'and',
                    'or',
                    'not'
                ],
                keywords: [
                    'return', 
                    'for', 
                    'while', 
                    'break',
                    'continue', 
                    'if', 
                    'else',
                ],
                operators: [
                    '=', '>', '<', '!', '~', '?', ':', '==', '<=', '>=', '!=',
                    '&&', '||', '++', '--', '+', '-', '*', '/', '&', '|', '^', '%',
                    '<<', '>>', '>>>', '+=', '-=', '*=', '/=', '&=', '|=', '^=',
                    '%=', '<<=', '>>=', '>>>='
                ],
                symbols:  /[=><!~?:&|+\-*\/\^%]+/,
                tokenizer: {
                    root: [
                        [/#.*/, 'comment'],
                        [/([a-zA-Z_][a-zA-Z_0-9]*)(\()/, ['function', 'default']],
                        [/\d+\.[fF]/, 'number'],
                        [/\d*\.\d+([eE][\-+]?\d+)?[Ff]?/, 'number'],
                        [/0[xX][0-9a-fA-F]+[uU]/, 'number'],
                        [/0[xX][0-9a-fA-F]+/, 'number'],
                        [/\d+[uU]/, 'number'],
                        [/\d+/, 'number'],
                        [/[A-Z][a-zA-Z_]*[\w$]*/, 'class'],
                        [/[a-zA-Z_$][\w$]*/, { 
                            cases: {
                                '@keywords': 'keyword', 
                                '@types': 'types', 
                                '@default': 'identifier' 
                            } 
                        }],
                        [/[{}()\[\]]/, 'default'],
                        [/[;,.]/, 'default'],
                        [/@symbols/, { 
                            cases: { 
                                '@operators': 'operator', 
                                '@default': '' 
                            }
                        }],
                        { include: '@whitespace' },

                        [/"([^"\\]|\\.)*$/, "string.invalid" ],
                        [/"/,  { token: "string.quote", bracket: "@open", next: "@string" } ],

                        [/"[^\\"]"/, "string"],
                        [/"/, "string.invalid"]
                    ],

                    string: [
                        [/[^\\"]+/,  "string"],
                        [/\\./,      "string.escape.invalid"],
                        [/"/,        { token: "string.quote", bracket: "@close", next: "@pop" } ]
                    ],
    
                    whitespace: [
                        [/[ \t\r\n]+/, 'white'],
                    ],
                },
            });

            monaco.editor.defineTheme("cyth", {
                base: "vs-dark",
                inherit: true,
                rules: [
                    { token: 'keyword', foreground: 'f7a8ff' },
                    { token: 'types', foreground: '00c4ff' },
                    { token: 'identifier', foreground: '9cdcfe' },
                    { token: 'number', foreground: 'adfd84' },
                    { token: 'function', foreground: 'fde3a1' },
                    { token: 'comment', foreground: '00af1b' },
                    { token: 'operator', foreground: 'c0c0c0' },
                    { token: 'class', foreground: '92ffc7' },
                    { token: 'default', foreground: '909090' },
                ],
                colors: {
                    "editor.background": "#1d1d1d",
                    "editorLineNumber.foreground": "#858585",
                    "scrollbar.shadow": "#00000000"
                }
            });
                  
            this.errors = [];
            this.model = monaco.editor.createModel("", "cyth"); 

            this.editorDeltaDecorationsList = [];
            this.editorElement = document.getElementById("editor");
            this.editor = monaco.editor.create(this.editorElement, {
                theme: "cyth",
                lineNumbers: "on",
                model: this.model,
                automaticLayout: true,
                scrollBeyondLastLine: true,
                minimap: { enabled: false },
                fixedOverflowWidgets: true
            });
            
            this.editorTabs = new EditorTabs(this);
            this.editorConsole = new EditorConsole(this.model, this.editorTabs, this);    
            
            this.editor.layout();
            this.editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KEY_S, () => {});
            this.editor.onDidChangeModelContent(() => this.onInput());
            
            this.editorTabs.load();
        });
    }

    setText(text)
    {
        this.editor.setValue(text);
        this.editor.setScrollPosition({ scrollTop: 0 });
    }

    setReadOnly(value)
    {
        this.editor.updateOptions({readOnly: value});
    }

    onError(startLineNumber, startColumn, endLineNumber, endColumn, message)
    {
        this.errors.push({
            startLineNumber,
            startColumn,
            endLineNumber,
            endColumn,
            message: Module.UTF8ToString(message)
        });
    }

    onInput() 
    {
        this.errors.length = 0;
        
        const text = this.editor.getValue();
        window.run(text, false);
        
        this.editorTabs.setText(text);
        this.editorConsole.setBytecode(text);

        monaco.editor.setModelMarkers(this.model, "owner", this.errors);
    }
}

var Module = {
    preRun: [],
    onRuntimeInitialized: function() {
        window.set_error_callback =  Module.cwrap("set_error_callback", null, ["number"]);
        window.set_result_callback =  Module.cwrap("set_result_callback", null, ["number"]);
        window.run = Module.cwrap("run", null, ["string", "number"]);

        new Editor();
    },
};