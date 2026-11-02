/**
 * Click SDK Loader for the Web Platform.
 *
 * Use this in your new web app to always get the latest Click OS SDK,
 * AI Agent System Prompt, and Simulator directly from this GitHub repo,
 * with ZERO manual downloads or uploads!
 */

const CLICK_REPO_OWNER = 'prajjwalprajjwal';
const CLICK_REPO_NAME = 'Click';
const CLICK_BRANCH = 'main';

const RAW_BASE = `https://raw.githubusercontent.com/${CLICK_REPO_OWNER}/${CLICK_REPO_NAME}/${CLICK_BRANCH}/sdk`;

export async function fetchLatestClickSDK() {
  try {
    const [specRes, promptRes] = await Promise.all([
      fetch(`${RAW_BASE}/click_sdk_spec.json`, { cache: 'no-store' }),
      fetch(`${RAW_BASE}/CLICK_AI_AGENT_PROMPT.md`, { cache: 'no-store' })
    ]);

    if (!specRes.ok || !promptRes.ok) {
      throw new Error(`Failed to fetch SDK from GitHub (Status: ${specRes.status}/${promptRes.status})`);
    }

    const spec = await specRes.json();
    const agentPrompt = await promptRes.text();

    return {
      spec,
      agentPrompt,
      simulatorUrl: `${RAW_BASE}/ClickSimulator.js`,
      templateHUrl: `${RAW_BASE}/AppletTemplate.h`,
      templateCppUrl: `${RAW_BASE}/AppletTemplate.cpp`,
      fetchedAt: new Date().toISOString()
    };
  } catch (err) {
    console.error('[Click SDK Loader] Error fetching latest SDK:', err);
    throw err;
  }
}
