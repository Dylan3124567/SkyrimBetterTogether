const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const ts = require('typescript');
const source = fs.readFileSync('src/app/components/host/host.component.ts', 'utf8');
const output = ts.transpileModule(source, { compilerOptions: {
  module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020, experimentalDecorators: true,
} }).outputText;
const context = { exports: {}, require: () => ({ Component: () => () => {}, Output: () => () => {}, EventEmitter: class {} }) };
vm.runInNewContext(output, context);
const calls = [];
const host = new context.exports.HostComponent({ hostPassword: 'initial', startHosting: (...args) => calls.push(args) });
for (const password of ['1', 'test password', 'x "!&=\\', 'a'.repeat(64)]) {
  host.password = password;
  assert.equal(host.valid, true, password);
  host.start();
  assert.equal(calls.at(-1)[1], password);
}
const count = calls.length;
for (const password of ['', 'a'.repeat(65), 'bad\npassword', 'bad\0password', 'é']) {
  host.password = password;
  assert.equal(host.valid, false);
  host.start();
}
assert.equal(calls.length, count);
host.password = '1';
for (const port of [0, -1, 65536, 1.5, NaN]) { host.port = port; assert.equal(host.valid, false); }
console.log('PASS host validation, exact custom password forwarding, invalid passwords and ports');
