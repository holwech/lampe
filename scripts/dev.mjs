import { spawn } from "node:child_process";
import { watch } from "node:fs";
import { createServer } from "vite";

function build() {
  return new Promise((resolve, reject) => {
    const child = spawn(
      "uv",
      ["run", "--locked", "python", "scripts/build_simulator.py"],
      { stdio: "inherit" },
    );
    child.on("error", reject);
    child.on("exit", (code) =>
      code === 0
        ? resolve()
        : reject(new Error(`Simulator build exited ${code}`)),
    );
  });
}
await build();
const server = await createServer();
await server.listen();
server.printUrls();
let timer,
  rebuilding = false,
  pending = false;
async function rebuild() {
  if (rebuilding) {
    pending = true;
    return;
  }
  rebuilding = true;
  try {
    await build();
    server.ws.send({ type: "full-reload" });
  } catch (error) {
    console.error(error.message);
  } finally {
    rebuilding = false;
    if (pending) {
      pending = false;
      void rebuild();
    }
  }
}
for (const directory of ["lib", "simulator"]) {
  watch(directory, { recursive: true }, (_, filename) => {
    if (filename && /\.(cpp|h|def)$/.test(filename)) {
      clearTimeout(timer);
      timer = setTimeout(rebuild, 200);
    }
  });
}
