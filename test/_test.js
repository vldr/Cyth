import test from "node:test";
import assert from "assert";
import path from "path";
import fs from "fs/promises";
import child_process from "child_process";

Error.stackTraceLimit = Infinity;

const executable = process.argv[2];
const decoder = new TextDecoder("utf-8");
const encoder = new TextEncoder();
const files = await fs.readdir(import.meta.dirname);
const scripts = process.env.FILE ? process.env.FILE.split(",").filter(Boolean) : files.filter((f) => f.endsWith(".cy"));

for (const filename of scripts) {
  await test(filename, async () => {
    const fullPath = path.join(import.meta.dirname, filename);
    const text = await fs.readFile(fullPath, "utf-8");
    const expectedLogs = text
      .split("\n")
      .filter((line) => line.startsWith("#") && !line.startsWith("#!"))
      .map((line) =>
        line
          .substring(line.indexOf("#") + 1)
          .replaceAll("\r", "")
          .replaceAll("\\0", "\0")
          .replaceAll("\\t", "\t")
          .replaceAll("\\b", "\b")
          .replaceAll("\\n", "\n")
          .replaceAll("\\r", "\r")
          .replaceAll("\\f", "\f")
          .trimStart()
      );

    const expectedErrors = text
      .split("\n")
      .filter((line) => line.startsWith("#!"))
      .map((line) => {
        const matches = line.match(
          /^#!\s*([0-9]+):([0-9]+)-([0-9]+):([0-9]+) (.+)/
        );
        return {
          startLineNumber: parseInt(matches[1]),
          startColumn: parseInt(matches[2]),
          endLineNumber: parseInt(matches[3]),
          endColumn: parseInt(matches[4]),
          message: matches[5].replaceAll("\r", ""),
        };
      });

    const process = child_process.spawnSync(executable, [], { input: text });
    const logs = [];
    const status = process.status;
    const bytecode = process.stdout;
    const errors = process.stderr
      .toString()
      .trim()
      .split("\n")
      .filter(Boolean)
      .map((line) => {
        const matches = line.match(
          /^\(null\):([0-9]+):([0-9]+)-([0-9]+):([0-9]+): error: (.+)/
        );

        return matches
          ? {
            startLineNumber: parseInt(matches[1]),
            startColumn: parseInt(matches[2]),
            endLineNumber: parseInt(matches[3]),
            endColumn: parseInt(matches[4]),
            message: matches[5].replaceAll("\r", ""),
          }
          : line;
      });

    if (errors.length === 0) {
      function log(output) {
        if (typeof output === "object") {
          const at = result.instance.exports["string.at"];
          const length = result.instance.exports["string.length"];

          const array = new Uint8Array(length(output));
          for (let i = 0; i < array.byteLength; i++) {
            array[i] = at(output, i);
          }

          logs.push(decoder.decode(array));
        } else if (typeof output == "undefined") {
          logs.push("");
        } else {
          logs.push(String(output));
        }
      }

      let stdin = encoder.encode("Input");
      let args = [encoder.encode("First\0"), encoder.encode("Second\0")];

      const kv = {};
      const result = await WebAssembly.instantiate(bytecode, {
        wasi_snapshot_preview1: {
          args_sizes_get: (environCountPtr, environBufferSizePtr) => {
            const envByteLength = args.map(s => s.byteLength).reduce((sum, val) => sum + val);
            const countPtrBuffer = new Uint32Array(result.instance.exports.memory.buffer, environCountPtr, 1);
            const sizePtrBuffer = new Uint32Array(result.instance.exports.memory.buffer, environBufferSizePtr, 1);
            countPtrBuffer[0] = args.length;
            sizePtrBuffer[0] = envByteLength;

            return 0;
          },

          args_get: (environPtr, environBufferPtr) => {
            const envByteLength = args.map(s => s.byteLength).reduce((sum, val) => sum + val);
            const environsPtrBuffer = new Uint32Array(result.instance.exports.memory.buffer, environPtr, args.length);
            const environsBuffer = new Uint8Array(result.instance.exports.memory.buffer, environBufferPtr, envByteLength)

            for (let i = 0, offset = 0; i < args.length; i++) {
              const currentPtr = environBufferPtr + offset;
              environsPtrBuffer[i] = currentPtr;
              environsBuffer.set(args[i], offset)
              offset += args[i].byteLength;
            }

            return 0;
          },

          fd_close: () => { },
          fd_read: (fd, iovsPtr, iovsLength, bytesReadPtr) => {
            const memory = new Uint8Array(result.instance.exports.memory.buffer);
            const iovs = new Uint32Array(result.instance.exports.memory.buffer, iovsPtr, iovsLength * 2);

            if (fd === 0) {
              let bytesRead = 0;

              for (let i = 0; i < iovsLength * 2; i += 2) {
                const offset = iovs[i];
                const length = iovs[i + 1];
                const chunk = stdin.slice(0, length);

                stdin = stdin.slice(length);
                memory.set(chunk, offset);

                bytesRead += chunk.byteLength;

                if (stdin.length === 0)
                  break;
              }

              const dataView = new DataView(result.instance.exports.memory.buffer);
              dataView.setInt32(bytesReadPtr, bytesRead, true);
            }

            return 0;
          },

          fd_write: (fd, iovsPtr, iovsLength, bytesWrittenPtr) => {
            const iovs = new Uint32Array(result.instance.exports.memory.buffer, iovsPtr, iovsLength * 2);

            if (fd === 1) {
              let text = String();
              let bytesWritten = 0;

              for (let i = 0; i < iovsLength * 2; i += 2) {
                const offset = iovs[i];
                const length = iovs[i + 1];
                const textChunk = decoder.decode(new Int8Array(result.instance.exports.memory.buffer, offset, length));

                text += textChunk;
                bytesWritten += length;
              }

              const dataView = new DataView(result.instance.exports.memory.buffer);
              dataView.setInt32(bytesWrittenPtr, bytesWritten, true);

              logs.push(text);
            }

            return 0;
          },
        },

        env: {
          "log": log,

          "log<bool>": log,
          "log<int>": log,
          "log<float>": log,
          "log<string>": log,
          "log<char>": log,

          "log()": log,
          "log(int)": log,
          "log(float)": log,
          "log(string)": log,
          "log(char)": log,

          set: (key, value) => { kv[key] = value; },
          get: (key) => { return key in kv ? kv[key] : null; },
        },
      });

      result.instance.exports["<start>"]();
    }

    assert.deepStrictEqual(errors, expectedErrors);
    assert.deepStrictEqual(logs, expectedLogs);

    if (errors.length === 0)
      assert.deepStrictEqual(status, 0);
    else
      assert.notDeepStrictEqual(status, 0);
  });
}
