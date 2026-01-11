let textDecoder;
let module;
let instance;
let context;
let canvas;
let debug;

let startFunction;
let lengthFunction;
let atFunction;
let drawFunction;
let keyPressedFunction;
let keyReleasedFunction;
let mousePressedFunction;
let mouseReleasedFunction;
let mouseDraggedFunction;
let mouseMovedFunction;

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

function log(text) {
  if (typeof text === "object") {
    const array = new Uint8Array(lengthFunction(text));
    for (let i = 0; i < array.byteLength; i++) array[i] = atFunction(text, i);

    postMessage({ type: "print", text: textDecoder.decode(array) });
  } else {
    postMessage({ type: "print", text });
  }
}

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
          "log<bool>.void(bool)": log,
          "log<int>.void(int)": log,
          "log<float>.void(float)": log,
          "log<string>.void(string)": log,
          "log<char>.void(char)": log,
          "log.void()": log,
          "log.void(int)": log,
          "log.void(float)": log,
          "log.void(string)": log,
          "log.void(char)": log,
          "log.void(bool)": log,

          "size.void(int, int)": function (width, height) {
            canvas.width = width;
            canvas.height = height;

            if (drawFunction) {
              function render(time) {
                try {
                  drawFunction(time);
                  requestAnimationFrame(render);
                } catch (error) {
                  postError(error);
                }
              }

              requestAnimationFrame(render);
            }
          },

          "fill.void(int, int, int)": function (r, g, b) {
            context.fillStyle = `rgb(${r}, ${g}, ${b})`;
          },

          "clear.void()": function () {
            context.fillRect(0, 0, canvas.width, canvas.height);
          },

          "rect.void(int, int, int, int)": function (x, y, width, height) {
            context.fillRect(x, y, width, height);
          },

          "circle.void(int, int, int)": function (x, y, radius) {
            context.beginPath();
            context.arc(x, y, radius, 0, 2 * Math.PI, false);
            context.fill();
          },

          "millis.int()": function () {
            return performance.now();
          },

          "random.float()": Math.random,
          "sqrt.float(float)": Math.sqrt,
          "cos.float(float)": Math.cos,
          "sin.float(float)": Math.sin,
          "tan.float(float)": Math.tan,
          "atan.float(float)": Math.atan,
          "atan2.float(float, float)": Math.atan2,
          "pow.float(float, float)": Math.pow,
        },
      });

      startFunction = instance.exports["<start>"];
      lengthFunction = instance.exports["string.length"];
      atFunction = instance.exports["string.at"];
      drawFunction = instance.exports["draw.void(int)"];
      keyPressedFunction = instance.exports["keyPressed.void(char)"];
      keyReleasedFunction = instance.exports["keyReleased.void(char)"];
      mousePressedFunction = instance.exports["mousePressed.void(int, int)"];
      mouseReleasedFunction = instance.exports["mouseReleased.void(int, int)"];
      mouseDraggedFunction = instance.exports["mouseDragged.void(int, int)"];
      mouseMovedFunction = instance.exports["mouseMoved.void(int, int)"];

      try {
        startFunction();
      } catch (error) {
        postError(error);
      }

      if (!drawFunction) {
        postMessage({ type: "stop" });
      }

      break;
    }

    case "keydown": {
      if (keyPressedFunction) {
        try {
          keyPressedFunction(data.key);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "keyup": {
      if (keyReleasedFunction) {
        try {
          keyReleasedFunction(data.key);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "mousedown": {
      if (mousePressedFunction) {
        try {
          mousePressedFunction(data.x, data.y);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }

    case "mousemove": {
      if (data.buttons) {
        if (mouseDraggedFunction) {
          try {
            mouseDraggedFunction(data.x, data.y);
          } catch (error) {
            postError(error);
          }
        }
      } else {
        if (mouseMovedFunction) {
          try {
            mouseMovedFunction(data.x, data.y);
          } catch (error) {
            postError(error);
          }
        }
      }

      break;
    }

    case "mouseup": {
      if (mouseReleasedFunction) {
        try {
          mouseReleasedFunction(data.x, data.y);
        } catch (error) {
          postError(error);
        }
      }

      break;
    }
  }
};
