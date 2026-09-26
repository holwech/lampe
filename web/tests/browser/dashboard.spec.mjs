import { test, expect } from "@playwright/test";

test("automatic simulation, real rendering, pixel inspection and responsive layout", async ({
  page,
}, testInfo) => {
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(message.text());
  });
  await page.goto("/");
  await expect(page.locator(".program")).toHaveCount(8);
  await expect(page.locator("#lamp-canvas")).toHaveAttribute(
    "data-ready",
    "true",
  );
  await page.locator('[data-program="2"]').click();
  await expect(page.locator("#audio-mode")).toBeEnabled();
  await page.selectOption("#audio-mode", "steady");
  await page.locator("#audio-level").fill("100");
  await expect(page.locator("#pixel-hex")).toHaveText("#FF0000");
  await page
    .getByRole("button", { name: "Inspect pixel 7", exact: true })
    .click();
  await expect(page.locator("#pixel-name")).toHaveText("Pixel 07");
  await page.getByRole("button", { name: "Diffuser on" }).click();
  await expect(
    page.getByRole("button", { name: "Diffuser off" }),
  ).toHaveAttribute("aria-pressed", "false");
  await page.getByRole("button", { name: "Diffuser off" }).click();
  await page.locator('[data-program="5"]').click();
  await page.waitForTimeout(250);
  await page.screenshot({
    path: testInfo.outputPath("studio-desktop.png"),
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: testInfo.outputPath("studio-mobile.png"),
    fullPage: true,
  });
  expect(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  ).toBe(true);
  expect(errors).toEqual([]);
});

test("animation stays stopped in hidden tabs and resumes when visible", async ({
  page,
}) => {
  // Override only the visibility signal: the browser still delivers real RAFs,
  // so this catches a loop that keeps rendering in the background.
  await page.addInitScript(() => {
    window.simulationTest = { hidden: true, frames: 0 };
    Object.defineProperty(document, "hidden", {
      get: () => window.simulationTest.hidden,
    });
    Object.defineProperty(document, "visibilityState", {
      get: () => (window.simulationTest.hidden ? "hidden" : "visible"),
    });
    const request = window.requestAnimationFrame.bind(window);
    window.requestAnimationFrame = (callback) =>
      request((time) => {
        window.simulationTest.frames++;
        callback(time);
      });
  });
  const frames = () => page.evaluate(() => window.simulationTest.frames);
  const colors = () =>
    page
      .locator(".pixel > span")
      .evaluateAll((pixels) =>
        pixels.map((pixel) => pixel.getAttribute("style")),
      );
  const visibility = (hidden) =>
    page.evaluate((hidden) => {
      window.simulationTest.hidden = hidden;
      document.dispatchEvent(new Event("visibilitychange"));
    }, hidden);

  await page.goto("/");
  await expect(page.locator(".program")).toHaveCount(8);
  await page.waitForTimeout(200);
  expect(await frames()).toBe(0);
  await visibility(false);
  await page.getByRole("button", { name: "Rainbow", exact: true }).click();
  const initial = await colors();
  await expect.poll(colors).not.toEqual(initial);
  await visibility(true);
  const stopped = await frames();
  const held = await colors();
  await page.waitForTimeout(500);
  expect(await frames()).toBe(stopped);
  expect(await colors()).toEqual(held);
  await visibility(false);
  await expect.poll(frames).toBeGreaterThan(stopped);
  await expect.poll(colors).not.toEqual(held);
});

async function mockSerial(page, cancel = false) {
  await page.addInitScript(
    ({ cancel }) => {
      window.serialTest = {
        opened: 0,
        closed: 0,
        signals: null,
        controller: null,
      };
      Object.defineProperty(navigator, "serial", {
        configurable: true,
        value: {
          async requestPort() {
            if (cancel)
              throw new DOMException("No port selected", "NotFoundError");
            const stream = new ReadableStream({
              start(controller) {
                window.serialTest.controller = controller;
              },
            });
            return {
              readable: stream,
              async open(options) {
                window.serialTest.opened++;
                window.serialTest.options = options;
              },
              async close() {
                if (stream.locked)
                  throw new Error("Reader lock was not released");
                window.serialTest.closed++;
              },
              async setSignals(signals) {
                window.serialTest.signals = signals;
              },
            };
          },
        },
      });
    },
    { cancel },
  );
}

test("live frames, split serial packets, stale state, disconnect and return to simulation", async ({
  page,
}) => {
  await mockSerial(page);
  await page.goto("/");
  await expect(page.locator(".program")).toHaveCount(8);
  await page.getByRole("button", { name: "Live lamp", exact: true }).click();
  await expect(page.locator("#live-info")).toContainText("separate 5 V supply");
  await page.getByRole("button", { name: "Connect lamp", exact: true }).click();
  await expect(
    page.getByRole("button", { name: "Disconnect", exact: true }),
  ).toBeVisible();
  await expect(page.locator('[data-program="7"]')).toBeDisabled();
  const expectedHex = await page.evaluate(async () => {
    const { default: createLamp } = await import("/generated/lamp.js");
    const module = await createLamp();
    module._lamp_reset(42);
    module._lamp_select(7);
    module._lamp_advance(120, 0, 0);
    const p = module._lamp_frame();
    const bytes = module.HEAPU8.slice(p, p + 61);
    window.serialTest.controller.enqueue(bytes.slice(0, 17));
    window.serialTest.controller.enqueue(bytes.slice(17));
    return (
      "#" +
      Array.from(bytes.slice(12, 15), (value) =>
        value.toString(16).padStart(2, "0"),
      )
        .join("")
        .toUpperCase()
    );
  });
  await expect(page.locator('[data-program="7"]')).toHaveAttribute(
    "aria-pressed",
    "true",
  );
  await expect(page.locator("#pixel-hex")).toHaveText(expectedHex);
  await expect(page.locator("#stage-message")).toHaveText(
    "Signal paused · last frame held",
  );
  await page.evaluate(() =>
    window.serialTest.controller.error(new Error("Cable unplugged")),
  );
  await expect(page.locator("#notice")).toContainText("Cable unplugged");
  await expect(page.locator("#pixel-hex")).toHaveText(expectedHex);
  await expect(page.locator("#live-mode")).toHaveAttribute(
    "aria-pressed",
    "true",
  );
  await page.getByRole("button", { name: "Simulator", exact: true }).click();
  await expect(page.locator('[data-program="7"]')).toBeEnabled();
  expect(await page.evaluate(() => window.serialTest.options.baudRate)).toBe(
    115200,
  );
  expect(await page.evaluate(() => window.serialTest.closed)).toBe(1);
});

test("canceling the port picker leaves a usable dashboard", async ({
  page,
}) => {
  await mockSerial(page, true);
  await page.goto("/");
  await expect(page.locator(".program")).toHaveCount(8);
  await page.getByRole("button", { name: "Live lamp", exact: true }).click();
  await page.getByRole("button", { name: "Connect lamp", exact: true }).click();
  await expect(page.locator("#notice")).toContainText("No port selected");
  await expect(
    page.getByRole("button", { name: "Connect lamp", exact: true }),
  ).toBeEnabled();
  await page.getByRole("button", { name: "Simulator", exact: true }).click();
  await page.getByRole("button", { name: "Rainbow", exact: true }).click();
  const initial = await page.locator("#pixel-hex").textContent();
  await expect(page.locator("#pixel-hex")).not.toHaveText(initial);
});

test("manual disconnect releases the port and preserves simulation settings", async ({
  page,
}, testInfo) => {
  await mockSerial(page);
  await page.goto("/");
  await expect(page.locator(".program")).toHaveCount(8);
  await page.locator('[data-program="5"]').click();
  await page.getByRole("button", { name: "Live lamp", exact: true }).click();
  await page.getByRole("button", { name: "Connect lamp", exact: true }).click();
  await page.getByRole("button", { name: "Disconnect", exact: true }).click();
  expect(await page.evaluate(() => window.serialTest.closed)).toBe(1);
  await page.getByRole("button", { name: "Connect lamp", exact: true }).click();
  expect(await page.evaluate(() => window.serialTest.opened)).toBe(2);
  await page.getByRole("button", { name: "Simulator", exact: true }).click();
  expect(await page.evaluate(() => window.serialTest.closed)).toBe(2);
  await expect(page.locator('[data-program="5"]')).toHaveAttribute(
    "aria-pressed",
    "true",
  );
  await expect(page.locator('[data-program="5"]')).toBeEnabled();
  await page.getByRole("button", { name: "Diffuser on", exact: true }).click();
  await page.screenshot({
    path: testInfo.outputPath("studio-led-ring.png"),
    fullPage: true,
  });
});
