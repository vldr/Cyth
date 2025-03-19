onmessage = (event) => {
  const data = event.data;

  switch (data.type) {
    case "start": {
      try {
        const textDecoder = new TextDecoder("utf-8");
        const module = new WebAssembly.Module(data.bytecode);
        const instance = new WebAssembly.Instance(module, {
          env: {
            log: function (text) {
              if (typeof text === "object") {
                const length = instance.exports["string.length"];
                const at = instance.exports["string.at"];

                const array = new Uint8Array(length(text));
                for (let i = 0; i < array.byteLength; i++)
                  array[i] = at(text, i);

                postMessage({ type: "print", text: textDecoder.decode(array) });
              } else {
                postMessage({ type: "print", text });
              }
            },
          },
        });

        instance.exports["<start>"]();

        postMessage({ type: "stop" });
      } catch (error) {
        const textDecoder = new TextDecoder();
        const sourceMap = JSON.parse(textDecoder.decode(data.debug));
        const base64Digits =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        const decode = (str, index) => {
          let result = 0,
            shift = 0,
            digit,
            continuation;
          do {
            digit = base64Digits.indexOf(str[index++]);
            continuation = digit & 32;
            result += (digit & 31) << shift;
            shift += 5;
          } while (continuation);
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

        const regex = /wasm-function\[([0-9]+)\]:(0[xX][0-9a-fA-F]+)/g;
        const stackTrace = {};
        const stackTraceOffsets = [];

        for (const matches of error.stack.matchAll(regex)) {
          const location = sourceMap.functions[Number(matches[1])];
          const offset = Number(matches[2]);

          stackTrace[offset] = location;
          stackTraceOffsets.push(offset);
        }

        map(sourceMap, stackTrace);

        let errorMessage = error.message + "\n";
        for (const offset of stackTraceOffsets) {
          errorMessage +=
            "    at " +
            stackTrace[offset].replaceAll("<", "&lt;").replaceAll(">", "&gt;") +
            "\n";
        }

        postMessage({
          type: "error",
          message: error.message ? errorMessage : "Internal error.",
        });
      }

      break;
    }
  }
};
