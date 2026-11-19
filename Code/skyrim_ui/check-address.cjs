require('ts-node').register({ transpileOnly: true, compilerOptions: { module: 'commonjs' } });
const assert = require('node:assert/strict');
const { parseServerAddress: parse } = require('./src/app/models/server-address.ts');
for (const value of ['', 'host:0', 'host:65536', 'host:-1', 'host:abc', 'a b', 'a/b', 'http://host', 'user@host', 'host:', '-host', 'host..com', '999.1.1.1', '[bad]', '::1', '1.2.3']) {
  assert.equal(parse(value), null, value);
}
for (const [value, host, port] of [['localhost','localhost',10578], [' 192.168.1.2:12345 ', '192.168.1.2',12345], ['example.com:65535','example.com',65535], ['[::1]:10578','[::1]',10578], ['[2001:db8::1]','[2001:db8::1]',10578]]) {
  assert.deepEqual(parse(value), {host, port}, value);
}
console.log('PASS address validation: IPv4, IPv6, DNS, whitespace and invalid ports/URLs');
