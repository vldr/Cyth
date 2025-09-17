let textDecoder;
let module;
let instance;
let context;
let canvas;
let debug;

function postError(error) {
  const sourceMap = JSON.parse(textDecoder.decode(debug));
  const base64Digits =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  const decode = (str, index) => {
    let result = 0,
      shift = 0,
      digit,
      continuation;

    if (str) {
      do {
        digit = base64Digits.indexOf(str[index++]);
        continuation = digit & 32;
        result += (digit & 31) << shift;
        shift += 5;
      } while (continuation);
    }
    return { value: (result & 1 ? -1 : 1) * (result >> 1), index };
  };

  const map = (sourceMap, stackTrace) => {
    let generatedLine = 1,
      generatedColumn = 0,
      sourceIndex = 0,
      originalLine = 0,
      originalColumn = 0,
      nameIndex = 0;

    for (const segment of sourceMap.mappings.split(",")) {
      let i = 0,
        result = decode(segment, i);
      generatedColumn += result.value;
      let mapping = { line: generatedLine, column: generatedColumn };
      i = result.index;
      if (i < segment.length) {
        result = decode(segment, i);
        sourceIndex += result.value;
        i = result.index;
        result = decode(segment, i);
        originalLine += result.value;
        i = result.index;
        result = decode(segment, i);
        originalColumn += result.value;
        i = result.index;
        mapping.source = sourceIndex;
        mapping.originalLine = originalLine + 1;
        mapping.originalColumn = originalColumn;
        if (i < segment.length) {
          result = decode(segment, i);
          nameIndex += result.value;
          mapping.name = nameIndex;
        }

        if (mapping.column in stackTrace) {
          stackTrace[mapping.column] +=
            ":" + mapping.originalLine + ":" + mapping.originalColumn;
        }
      }
    }
  };

  let errorMessage = error.message + "\n";

  const regex = /wasm-function\[([0-9]+)\]:(0[xX][0-9a-fA-F]+)/g;
  const stack = error.stack;

  if (regex.test(stack)) {
    const stackTrace = {};
    const stackTraceOffsets = [];
    regex.lastIndex = 0;

    for (const matches of stack.matchAll(regex)) {
      const location = sourceMap.functions[Number(matches[1])];
      const offset = Number(matches[2]);

      stackTrace[offset] = location;
      stackTraceOffsets.push(offset);
    }

    map(sourceMap, stackTrace);

    for (const offset of stackTraceOffsets) {
      let error = stackTrace[offset].replaceAll("<", "&lt;").replaceAll(">", "&gt;");

      const regex = /.*:([0-9]+):([0-9]+)/;
      const matches = error.match(regex);
      if (matches)
        error = `<a onclick="Module.editor.goto(${matches[1]}, ${matches[2]})" href="#">${error}</a>`;

      errorMessage +=
        "    at " + error + "\n";
    }
  }
  else {
    const regex = /wasm-function\[([0-9]+)\]/g;

    for (const matches of stack.matchAll(regex)) {
      const location = sourceMap.functions[Number(matches[1])];
      errorMessage +=
        "    at " +
        location.replaceAll("<", "&lt;").replaceAll(">", "&gt;") +
        "\n";
    }
  }

  postMessage({
    type: "error",
    message: error.message ? errorMessage : "Internal error.",
  });
};

onmessage = (event) => {
  const data = event.data;

  switch (data.type) {
    case "start": {
      debug = data.debug;
      canvas = data.canvas;
      context = canvas.getContext("2d");
      textDecoder = new TextDecoder("utf-8");
      module = new WebAssembly.Module(data.bytecode);
      instance = new WebAssembly.Instance(module, {
        env: {
          log: function (text) {
            if (typeof text === "object") {
              const length = instance.exports["string.length"];
              const at = instance.exports["string.at"];

              const array = new Uint8Array(length(text));
              for (let i = 0; i < array.byteLength; i++) array[i] = at(text, i);

              postMessage({ type: "print", text: textDecoder.decode(array) });
            } else {
              postMessage({ type: "print", text });
            }
          },

          size: function (width, height) {
            canvas.width = width;
            canvas.height = height;

            if (instance.exports["draw"]) {
              function render(time) {
                try {
                  instance.exports["draw"](time);
                  requestAnimationFrame(render);
                } catch (error) {
                  postError(error);
                }
              }

              requestAnimationFrame(render);
            }
          },

          fill: function (r, g, b) {
            context.fillStyle = `rgb(${r}, ${g}, ${b})`;
          },

          clear: function () {
            context.fillRect(0, 0, canvas.width, canvas.height);
          },

          rect: function (x, y, width, height) {
            context.fillRect(x, y, width, height);
          },

          circle: function (x, y, radius) {
            context.beginPath();
            context.arc(x, y, radius, 0, 2 * Math.PI, false);
            context.fill();
          },

          millis: function () {
            return performance.now();
          },

          random: Math.random,
          sqrt: Math.sqrt,
          cos: Math.cos,
          sin: Math.sin,
          tan: Math.tan,
          atan: Math.atan,
          atan2: Math.atan2,
          pow: Math.pow,
        },
      });

      try {
        instance.exports["<start>"]();
      } catch (error) {
        postError(error);
      }

      if (!instance.exports["draw"]) {
        postMessage({ type: "stop" });
      }

      break;
    }

    case "keydown": {
      if (instance.exports["keyPressed"]) {
        try {
          instance.exports["keyPressed"](data.key);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "keyup": {
      if (instance.exports["keyReleased"]) {
        try {
          instance.exports["keyReleased"](data.key);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "mousedown": {
      if (instance.exports["mousePressed"]) {
        try {
          instance.exports["mousePressed"](data.x, data.y);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "mousemove": {
      if (data.buttons) {
        if (instance.exports["mouseDragged"]) {
          try {
            instance.exports["mouseDragged"](data.x, data.y);
          } catch (error) {
            postError(error);
          }
        }
      } else {
        if (instance.exports["mouseMoved"]) {
          try {
            instance.exports["mouseMoved"](data.x, data.y);
          } catch (error) {
            postError(error);
          }
        }
      }

      break;
    }

    case "mouseup": {
      if (instance.exports["mouseReleased"]) {
        try {
          instance.exports["mouseReleased"](data.x, data.y);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }
  }
};
