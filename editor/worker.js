let textDecoder;
let module;
let instance;
let context;
let canvas;
let debug;

function postError(error) {
  function decodeMappings(mappings) {
    const A = "A".charCodeAt(0);
    const Z = "Z".charCodeAt(0);
    const a = "a".charCodeAt(0);
    const z = "z".charCodeAt(0);
    const zero = "0".charCodeAt(0);
    const nine = "9".charCodeAt(0);
    const plus = "+".charCodeAt(0);
    const slash = "/".charCodeAt(0);
    const comma = ",".charCodeAt(0);

    let offset = 0;
    let file = 0;
    let line = 0;
    let column = 0;
    let index = 0;
    let result = [];

    while (index < mappings.length) {
      function decodeVLQ() {
        let output = 0;
        let shift = 0;
        let sign = 1;

        while (true) {
          if (index >= mappings.length)
            break;

          const character = mappings.charCodeAt(index);
          if (character == comma)
            break;

          let value;
          if (character >= A && character <= Z)
            value = character - A;
          else if (character >= a && character <= z)
            value = 26 + (character - a);
          else if (character >= zero && character <= nine)
            value = 52 + (character - zero);
          else if (character == plus)
            value = 62;
          else if (character == slash)
            value = 63;
          else
            throw new Error("Unknown character");

          const continuation = value & 0b100000;

          if (shift == 0) {
            if (value & 0b1)
              sign = -sign;

            const digit = (value >>> 1) & 0b1111;
            output |= digit;
            shift += 4;
          }
          else {
            const digit = value & 0b11111;
            output |= digit << shift;
            shift += 5;
          }

          index++;

          if (!continuation)
            break;
        }

        return sign * output;
      }

      offset += decodeVLQ();
      file += decodeVLQ();
      line += decodeVLQ();
      column += decodeVLQ();
      decodeVLQ();

      result[offset] = { line, column };

      if (index < mappings.length && mappings.charCodeAt(index) != comma)
        throw new Error("Expected a comma");

      index++;
    }

    return result;
  }

  let errorMessage = error.message + "\n";

  const sourceMap = JSON.parse(textDecoder.decode(debug));
  const regex = /wasm-function\[([0-9]+)\]:(0[xX][0-9a-fA-F]+)/g;
  const stack = error.stack;

  if (regex.test(stack)) {
    const strackTrace = [];
    regex.lastIndex = 0;

    for (const matches of stack.matchAll(regex)) {
      const location = sourceMap.functions[Number(matches[1])];
      const offset = Number(matches[2]);

      strackTrace.push([offset, location]);
    }

    const mappings = decodeMappings(sourceMap.mappings);

    for (const [offset, location] of strackTrace) {
      let error = location.replaceAll("<", "&lt;").replaceAll(">", "&gt;");

      const mapping = mappings[offset];
      if (mapping)
        error = `<a onclick="Module.editor.goto(${mapping.line + 1}, ${mapping.column})" href="#">${error}:${mapping.line + 1}:${mapping.column}</a>`;

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
