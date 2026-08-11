'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const library = {};
const hostHeap = new Uint8Array(4096);
let stackCursor = 2048;
const subscriptions = [];
const context = {
  LibraryManager: { library },
  mergeInto(target, values) { Object.assign(target, values); },
  HEAPU8: hostHeap,
  TextDecoder,
  TextEncoder,
  WebAssembly,
  console: { log() {}, warn() {}, error() {} },
  UTF8ToString(value) { return String(value); },
  stringToUTF8(value, out, cap) {
    const bytes = new TextEncoder().encode(value).subarray(0, Math.max(0, cap - 1));
    hostHeap.set(bytes, out);
    hostHeap[out + bytes.length] = 0;
  },
  stackSave() { return stackCursor; },
  stackAlloc(size) { const result = stackCursor; stackCursor += size; return result; },
  stackRestore(saved) { stackCursor = saved; },
  _woki_web_host_event_subscribe(handle, type) {
    subscriptions.push([handle, type]);
    return handle === 11 ? 0 : -2;
  },
  _woki_web_host_event_emit() { return 0; },
  _woki_web_host_event_subscribe_named() { return 0; },
  _woki_web_host_event_emit_named() { return 0; },
  FS: {
    isLink() { return false; },
    lookupPath() { throw { errno: 44 }; },
    mkdir() {},
    readFile() { return new Uint8Array([97, 0, 98]); },
    writeFile() {},
  },
};
vm.createContext(context);
const source = fs.readFileSync(path.join(__dirname, 'woki_ext.js'), 'utf8');
vm.runInContext(source, context, { filename: 'woki_ext.js' });
context.WokiExt = library.$WokiExt;
context.WokiExtContract = library.$WokiExtContract;

const contract = context.WokiExtContract;
const bridge = context.WokiExt;
function record(handle, permissions, hostHandle) {
  return {
    handle,
    id: handle,
    permissions,
    hostHandle,
    dataPath: `/data/${handle}`,
    configPath: `/config/${handle}`,
    cachePath: `/cache/${handle}`,
    instance: { exports: { memory: new WebAssembly.Memory({ initial: 1 }) } },
  };
}

const first = record('first', contract.permission.log | contract.permission.config | contract.permission.events, 11);
const second = record('second', 0, 22);
assert.equal(bridge.bytes(first, 65536, 0).length, 0);
assert.throws(() => bridge.bytes(first, 65536, 1), /out of bounds/);
assert.throws(() => bridge.bytes(first, -1, 1), /out of bounds/);

const firstHost = bridge.imports(first).woki_host;
const secondHost = bridge.imports(second).woki_host;
assert.equal(secondHost.host_log(0, 0, 0), contract.status.denied);
assert.equal(firstHost.host_log(0, 65536, 1), contract.status.invalid);
assert.equal(firstHost.host_log(0, 0, contract.limits.log + 1), contract.status.noSpace);
const firstMemory = new Uint8Array(first.instance.exports.memory.buffer);
firstMemory.set([107, 101, 121, 0], 0);
firstMemory.set([97, 0, 98], 16);
assert.equal(firstHost.host_config_set(0, 16, 3), contract.status.invalid);
assert.equal(firstHost.host_config_get(0, 32, 16), contract.status.invalid);
assert.equal(secondHost.host_file_write_n(0, 1, 0, 0), contract.status.denied);
assert.equal(firstHost.host_event_subscribe(7), contract.status.ok);
assert.equal(secondHost.host_event_subscribe(8), contract.status.denied);
assert.deepEqual(subscriptions, [[11, 7], [22, 8]]);
assert.match(bridge.errors.get('second'), /host_event_subscribe failed with status -2/);
bridge.clearError(first);
bridge.clearError(second);

bridge.instances.set('first', first);
bridge.instances.set('second', second);
first.instance.exports.ext_init = () => contract.status.invalid;
second.instance.exports.ext_init = () => contract.status.ok;
assert.equal(library.woki_web_ext_init('first'), contract.status.invalid);
assert.equal(library.woki_web_ext_init('second'), contract.status.ok);
assert.match(bridge.errors.get('first'), /ext_init returned -5/);
assert.equal(bridge.errors.has('second'), false);
library.woki_web_ext_discard('first');
assert.equal(bridge.instances.has('first'), false);
assert.equal(bridge.instances.has('second'), true);
assert.equal(bridge.errors.has('first'), false);

console.log('web extension bridge behavioral checks passed');
