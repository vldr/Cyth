onmessage = (event) => 
{
    const data = event.data;

    switch (data.type)
    {
        case "start": 
        {
            try 
            {
                const textDecoder = new TextDecoder("utf-8");
                const module = new WebAssembly.Module(data.bytecode);
                const instance = new WebAssembly.Instance(module, {
                    env: {
                        log: function(text) {
                            if (typeof(text) === "object")
                            {
                                const length = instance.exports["string.length"];
                                const at = instance.exports["string.at"];

                                const array = new Uint8Array(length(text));
                                for (let i = 0; i < array.byteLength; i++)
                                    array[i] = at(text, i);

                                postMessage({ type: "print", text: textDecoder.decode(array) });
                            }
                            else 
                            {
                                postMessage({ type: "print", text });
                            }
                        }
                    }
                });

                instance.exports["~start"]();

                postMessage({ type: "stop" });
            }
            catch (error)
            {
                postMessage({ type: "error", message: error.message ? error.message : "Internal error." });
            }

            break;
        }
    }
}