import Module from "../output/cyth.js";
import { expect, test } from "bun:test";
import { Glob } from "bun";

await new Promise(resolve => Module["onRuntimeInitialized"] = resolve);

Module.set_error_callback = Module.cwrap("set_error_callback", null, [
  "number",
]);

Module.set_result_callback = Module.cwrap("set_result_callback", null, [
  "number",
]);

let errors = [];

Module.set_result_callback(Module.addFunction(() => {}, "vii"));
Module.set_error_callback(Module.addFunction((startLineNumber, startColumn, endLineNumber, endColumn, message) => errors.push({
  startLineNumber,
  startColumn,
  endLineNumber,
  endColumn,
  message: Module.UTF8ToString(message)
}), "viiiii"));

const glob = new Glob("input/*.cy");
const run = Module.cwrap("run", null, ["string", "number"]);

for await (const path of glob.scan(".")) {
  test(path, async () => {
    const file = Bun.file(path);
    const text = await file.text();
    errors.length = 0;

    run(text, false);
    expect(errors).toBeEmpty();
  });
}



