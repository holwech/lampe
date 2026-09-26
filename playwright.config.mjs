import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "./web/tests/browser",
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  projects: [
    { name: "development", use: { baseURL: "http://127.0.0.1:5173" } },
    { name: "production-preview", use: { baseURL: "http://127.0.0.1:4173" } },
  ],
  use: {
    viewport: { width: 1440, height: 1100 },
    // Capturing every passing WebGL interaction adds significant CI overhead.
    trace: process.env.CI ? "on-first-retry" : "retain-on-failure",
    launchOptions: { args: ["--enable-unsafe-swiftshader"] },
  },
  webServer: [
    {
      command: "npm run dev",
      url: "http://127.0.0.1:5173",
      reuseExistingServer: !process.env.CI,
    },
    {
      command: "npm run preview",
      url: "http://127.0.0.1:4173",
      reuseExistingServer: !process.env.CI,
    },
  ],
});
