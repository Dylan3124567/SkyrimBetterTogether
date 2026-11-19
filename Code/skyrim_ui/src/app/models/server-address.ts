export function parseServerAddress(value: string): { host: string; port: number } | null {
  const match = value.trim().match(/^(\[[0-9a-fA-F:.]+\]|[a-zA-Z0-9.-]+)(?::([0-9]+))?$/);
  if (!match) return null;
  const port = match[2] === undefined ? 10578 : Number(match[2]);
  if (!Number.isInteger(port) || port < 1 || port > 65535) return null;
  const host = match[1];
  if (host.length > 253) return null;
  if (!host.startsWith('[')) {
    if (!host.split('.').every(label => /^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$/.test(label))) return null;
    if (/^[0-9.]+$/.test(host) && !/^(?:\d{1,3}\.){3}\d{1,3}$/.test(host)) return null;
    if (/^[0-9.]+$/.test(host) && host.split('.').some(part => Number(part) > 255)) return null;
  }
  try { new URL(`http://${host}:${port}`); } catch { return null; }
  return { host, port };
}
