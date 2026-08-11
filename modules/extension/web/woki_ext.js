mergeInto(LibraryManager.library, {
  // Static lowering of sdk/abi.h. sdk/check_contract.py prevents drift.
  $WokiExtContract: Object.freeze({
    limits: Object.freeze({
      log: 4096,
      configKey: 128,
      configValue: 64 * 1024,
      path: 4096,
      event: 64 * 1024,
      eventTopic: 255,
      file: 16 * 1024 * 1024,
      memoryPages: 512,
    }),
    status: Object.freeze({
      ok: 0,
      err: -1,
      denied: -2,
      noSpace: -3,
      notFound: -4,
      invalid: -5,
    }),
    permission: Object.freeze({
      log: 1 << 0,
      paths: 1 << 1,
      storage: 1 << 2,
      config: 1 << 3,
      events: 1 << 4,
    }),
    requiredExports: Object.freeze([
      'memory', 'ext_api_version', 'ext_init', 'ext_on_tick', 'ext_on_event', 'ext_on_unload',
    ]),
    hostImports: Object.freeze([
      'host_log', 'host_path_data', 'host_path_cache',
      'host_file_read', 'host_file_write', 'host_file_append',
      'host_file_read_n', 'host_file_write_n', 'host_file_append_n',
      'host_config_get', 'host_config_set', 'host_event_subscribe', 'host_event_emit',
      'host_event_subscribe_named', 'host_event_emit_named',
    ]),
  }),

  $WokiExt__deps: ['$WokiExtContract', '$UTF8ToString', '$stackSave', '$stackAlloc', '$stackRestore'],
  $WokiExt: {
    instances: new Map(),
    errors: new Map(),
    setError(session, message, status = WokiExtContract.status.err) {
      const handle = typeof session === 'string' ? session : session.handle;
      this.errors.set(handle, String(message || 'unknown web extension error'));
      return status;
    },
    clearError(session) {
      const handle = typeof session === 'string' ? session : session.handle;
      this.errors.delete(handle);
    },
    has(record, permission) {
      return (record.permissions & permission) !== 0;
    },
    memory(record) {
      return record.instance.exports.memory;
    },
    bytes(record, ptr, len) {
      ptr >>>= 0;
      len >>>= 0;
      const memory = this.memory(record);
      if (!memory) {
        throw new Error('guest module does not export memory');
      }
      const view = new Uint8Array(memory.buffer);
      if (ptr > view.length || len > view.length - ptr) {
        throw new Error('guest memory access is out of bounds');
      }
      return view.subarray(ptr, ptr + len);
    },
    hostBytes(ptr, len) {
      ptr >>>= 0;
      len >>>= 0;
      if (ptr > HEAPU8.length || len > HEAPU8.length - ptr) {
        throw new Error('host memory access is out of bounds');
      }
      return HEAPU8.subarray(ptr, ptr + len);
    },
    text(record, ptr, len) {
      return new TextDecoder().decode(this.bytes(record, ptr, len));
    },
    cstr(record, ptr) {
      ptr >>>= 0;
      const memory = this.memory(record);
      if (!memory) {
        throw new Error('invalid guest string pointer');
      }
      const view = new Uint8Array(memory.buffer);
      if (ptr >= view.length) {
        throw new Error('guest string pointer is out of bounds');
      }
      let end = ptr;
      const max = Math.min(view.length, ptr + WokiExtContract.limits.path + 1);
      while (end < max && view[end] !== 0) {
        ++end;
      }
      if (end === max) {
        throw new Error('guest string is not null terminated');
      }
      return new TextDecoder().decode(view.subarray(ptr, end));
    },
    writeText(record, ptr, cap, value) {
      cap >>>= 0;
      if (cap === 0) return WokiExtContract.status.invalid;
      const out = this.bytes(record, ptr, cap);
      const encoded = new TextEncoder().encode(String(value));
      if (encoded.length + 1 > cap) {
        return WokiExtContract.status.noSpace;
      }
      out.set(encoded, 0);
      out[encoded.length] = 0;
      return WokiExtContract.status.ok;
    },
    readU32(record, ptr) {
      const bytes = this.bytes(record, ptr, 4);
      return (bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24)) >>> 0;
    },
    writeU32(record, ptr, value) {
      const bytes = this.bytes(record, ptr, 4);
      bytes[0] = value & 0xff;
      bytes[1] = (value >> 8) & 0xff;
      bytes[2] = (value >> 16) & 0xff;
      bytes[3] = (value >> 24) & 0xff;
    },
    ensureDir(path) {
      const parts = path.split('/').filter(Boolean);
      let current = '';
      for (const part of parts) {
        current += '/' + part;
        try {
          if (this.isSymlink(current)) throw new Error('symbolic links are not allowed in extension storage paths');
          FS.mkdir(current);
        } catch (error) {
          if (!error || error.errno !== 20) {
            throw error;
          }
          if (this.isSymlink(current)) throw new Error('symbolic links are not allowed in extension storage paths');
        }
      }
    },
    isSymlink(path) {
      try {
        return FS.isLink(FS.lookupPath(path, { follow: false }).node.mode);
      } catch (error) {
        if (error && error.errno === 44) return false;
        throw error;
      }
    },
    safePath(root, rel) {
      if (!rel || rel.length > WokiExtContract.limits.path || rel.startsWith('/') || rel.includes('\\') || rel.includes('\0')) {
        return null;
      }
      const parts = rel.split('/');
      if (parts.some((part) => !part || part === '.' || part === '..')) return null;
      const base = root.replace(/\/+$/, '');
      let current = base;
      if (this.isSymlink(current)) return null;
      for (const part of parts) {
        current += '/' + part;
        if (this.isSymlink(current)) return null;
      }
      return current;
    },
    safeConfigKey(key) {
      return key.length > 0 && key.length <= WokiExtContract.limits.configKey
        && key !== '.' && key !== '..' && /^[A-Za-z0-9_.-]+$/.test(key);
    },
    imports(record) {
      const host = {};
      host.host_log = (level, ptr, len) => {
        if (!this.has(record, WokiExtContract.permission.log)) {
          return WokiExtContract.status.denied;
        }
        try {
          if (len > WokiExtContract.limits.log) return WokiExtContract.status.noSpace;
          const message = this.text(record, ptr, len);
          const prefix = `[${record.id}]`;
          if (level === 3) console.error(prefix, message);
          else if (level === 2) console.warn(prefix, message);
          else console.log(prefix, message);
          return WokiExtContract.status.ok;
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_path_data = (outPtr, outCap) => {
        if (!this.has(record, WokiExtContract.permission.paths)) return WokiExtContract.status.denied;
        try {
          this.ensureDir(record.dataPath);
          return this.writeText(record, outPtr, outCap, record.dataPath);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_path_cache = (outPtr, outCap) => {
        if (!this.has(record, WokiExtContract.permission.paths)) return WokiExtContract.status.denied;
        try {
          this.ensureDir(record.cachePath);
          return this.writeText(record, outPtr, outCap, record.cachePath);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      const readFile = (path, outPtr, inoutLenPtr) => {
        if (!this.has(record, WokiExtContract.permission.storage)) return WokiExtContract.status.denied;
        const fullPath = this.safePath(record.dataPath, path);
        if (!fullPath) return WokiExtContract.status.invalid;
        let data;
        try {
          data = FS.readFile(fullPath);
        } catch (error) {
          return WokiExtContract.status.notFound;
        }
        if (data.length > WokiExtContract.limits.file) return WokiExtContract.status.noSpace;
        try {
          const cap = this.readU32(record, inoutLenPtr);
          this.writeU32(record, inoutLenPtr, data.length);
          if (cap < data.length) return WokiExtContract.status.noSpace;
          this.bytes(record, outPtr, cap).set(data, 0);
          return WokiExtContract.status.ok;
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_file_read = (pathPtr, outPtr, inoutLenPtr) => {
        try {
          return readFile(this.cstr(record, pathPtr), outPtr, inoutLenPtr);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_file_read_n = (pathPtr, pathLen, outPtr, inoutLenPtr) => {
        try {
          if (pathLen > WokiExtContract.limits.path) return WokiExtContract.status.noSpace;
          return readFile(this.text(record, pathPtr, pathLen), outPtr, inoutLenPtr);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      const writeFile = (path, dataPtr, dataLen, append) => {
        if (!this.has(record, WokiExtContract.permission.storage)) return WokiExtContract.status.denied;
        if (dataLen > WokiExtContract.limits.file) return WokiExtContract.status.noSpace;
        const fullPath = this.safePath(record.dataPath, path);
        if (!fullPath) return WokiExtContract.status.invalid;
        let bytes;
        try {
          bytes = this.bytes(record, dataPtr, dataLen);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
        try {
          const parent = fullPath.substring(0, fullPath.lastIndexOf('/'));
          this.ensureDir(parent);
          if (append) {
            let old = new Uint8Array();
            try {
              old = FS.readFile(fullPath);
            } catch (_) {
            }
            if (old.length > WokiExtContract.limits.file - bytes.length) return WokiExtContract.status.noSpace;
            const merged = new Uint8Array(old.length + bytes.length);
            merged.set(old, 0);
            merged.set(bytes, old.length);
            FS.writeFile(fullPath, merged);
          } else {
            FS.writeFile(fullPath, bytes);
          }
          return WokiExtContract.status.ok;
        } catch (error) {
          return this.setError(record, error.message);
        }
      };
      host.host_file_write = (pathPtr, dataPtr, dataLen) => {
        try {
          return writeFile(this.cstr(record, pathPtr), dataPtr, dataLen, false);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_file_append = (pathPtr, dataPtr, dataLen) => {
        try {
          return writeFile(this.cstr(record, pathPtr), dataPtr, dataLen, true);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_file_write_n = (pathPtr, pathLen, dataPtr, dataLen) => {
        try {
          if (pathLen > WokiExtContract.limits.path) return WokiExtContract.status.noSpace;
          return writeFile(this.text(record, pathPtr, pathLen), dataPtr, dataLen, false);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_file_append_n = (pathPtr, pathLen, dataPtr, dataLen) => {
        try {
          if (pathLen > WokiExtContract.limits.path) return WokiExtContract.status.noSpace;
          return writeFile(this.text(record, pathPtr, pathLen), dataPtr, dataLen, true);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_config_get = (keyPtr, outPtr, outCap) => {
        if (!this.has(record, WokiExtContract.permission.config)) return WokiExtContract.status.denied;
        let key;
        try { key = this.cstr(record, keyPtr); } catch (error) { return WokiExtContract.status.invalid; }
        if (key.length > WokiExtContract.limits.configKey) return WokiExtContract.status.noSpace;
        if (!this.safeConfigKey(key)) return WokiExtContract.status.invalid;
        const path = this.safePath(record.configPath, key);
        if (!path) return WokiExtContract.status.invalid;
        let data;
        try { data = FS.readFile(path); } catch (error) { return WokiExtContract.status.notFound; }
        if (data.length > WokiExtContract.limits.configValue) return WokiExtContract.status.noSpace;
        if (data.includes(0)) return WokiExtContract.status.invalid;
        try {
          return this.writeText(record, outPtr, outCap, new TextDecoder().decode(data));
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_config_set = (keyPtr, valuePtr, valueLen) => {
        if (!this.has(record, WokiExtContract.permission.config)) return WokiExtContract.status.denied;
        if (valueLen > WokiExtContract.limits.configValue) return WokiExtContract.status.noSpace;
        let key;
        let value;
        try {
          key = this.cstr(record, keyPtr);
          value = this.bytes(record, valuePtr, valueLen);
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
        if (value.includes(0)) return WokiExtContract.status.invalid;
        if (key.length > WokiExtContract.limits.configKey) return WokiExtContract.status.noSpace;
        if (!this.safeConfigKey(key)) return WokiExtContract.status.invalid;
        const path = this.safePath(record.configPath, key);
        if (!path) return WokiExtContract.status.invalid;
        try {
          this.ensureDir(path.substring(0, path.lastIndexOf('/')));
          FS.writeFile(path, value);
          return WokiExtContract.status.ok;
        } catch (error) {
          return this.setError(record, error.message);
        }
      };
      host.host_event_subscribe = (eventType) =>
        _woki_web_host_event_subscribe(record.hostHandle, eventType);
      host.host_event_emit = (eventType, payloadPtr, payloadLen) => {
        if (payloadLen > WokiExtContract.limits.event) return WokiExtContract.status.noSpace;
        try {
          const payload = this.bytes(record, payloadPtr, payloadLen);
          const saved = stackSave();
          try {
            const hostPayload = payloadLen ? stackAlloc(payloadLen) : 0;
            if (payloadLen) HEAPU8.set(payload, hostPayload);
            return _woki_web_host_event_emit(record.hostHandle, eventType, hostPayload, payloadLen);
          } finally {
            stackRestore(saved);
          }
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_event_subscribe_named = (namePtr, nameLen) => {
        if (nameLen > WokiExtContract.limits.eventTopic) return WokiExtContract.status.noSpace;
        try {
          const name = this.bytes(record, namePtr, nameLen);
          const saved = stackSave();
          try {
            const hostName = nameLen ? stackAlloc(nameLen) : 0;
            if (nameLen) HEAPU8.set(name, hostName);
            return _woki_web_host_event_subscribe_named(record.hostHandle, hostName, nameLen);
          } finally {
            stackRestore(saved);
          }
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      host.host_event_emit_named = (namePtr, nameLen, payloadPtr, payloadLen) => {
        if (nameLen > WokiExtContract.limits.eventTopic || payloadLen > WokiExtContract.limits.event) return WokiExtContract.status.noSpace;
        try {
          const name = this.bytes(record, namePtr, nameLen);
          const payload = this.bytes(record, payloadPtr, payloadLen);
          const saved = stackSave();
          try {
            const hostName = nameLen ? stackAlloc(nameLen) : 0;
            const hostPayload = payloadLen ? stackAlloc(payloadLen) : 0;
            if (nameLen) HEAPU8.set(name, hostName);
            if (payloadLen) HEAPU8.set(payload, hostPayload);
            return _woki_web_host_event_emit_named(record.hostHandle, hostName, nameLen, hostPayload, payloadLen);
          } finally {
            stackRestore(saved);
          }
        } catch (error) {
          return WokiExtContract.status.invalid;
        }
      };
      for (const [name, callback] of Object.entries(host)) {
        host[name] = (...args) => {
          let status;
          try {
            status = callback(...args);
          } catch (error) {
            status = WokiExtContract.status.invalid;
          }
          if (status !== WokiExtContract.status.ok) {
            this.setError(record, `${name} failed with status ${status}`, status);
          }
          return status;
        };
      }
      return { woki_host: host };
    },
  },

  woki_web_ext_load__deps: ['$WokiExt'],
  woki_web_ext_load: function(handlePtr, idPtr, wasmBytesPtr, wasmBytesLen, dataPathPtr, configPathPtr, cachePathPtr, permissions, hostHandle) {
    const handle = UTF8ToString(handlePtr);
    const id = UTF8ToString(idPtr);
    try {
      if (WokiExt.instances.has(handle)) {
        return WokiExt.setError(handle, `web extension session ${handle} is already loaded`);
      }
      const record = {
        handle,
        id,
        dataPath: UTF8ToString(dataPathPtr),
        configPath: UTF8ToString(configPathPtr),
        cachePath: UTF8ToString(cachePathPtr),
        permissions,
        hostHandle,
        instance: null,
      };
      const bytes = WokiExt.hostBytes(wasmBytesPtr, wasmBytesLen).slice();
      const module = new WebAssembly.Module(bytes);
      for (const imported of WebAssembly.Module.imports(module)) {
        if (imported.kind !== 'function' || imported.module !== 'woki_host'
            || !WokiExtContract.hostImports.includes(imported.name)) {
          return WokiExt.setError(handle, `unknown extension import ${imported.module}::${imported.name}`);
        }
      }
      const instance = new WebAssembly.Instance(module, WokiExt.imports(record));
      record.instance = instance;
      const exports = instance.exports;
      for (const name of WokiExtContract.requiredExports) {
        if (!exports[name]) {
          return WokiExt.setError(handle, `missing export ${name}`);
        }
      }
      if ((!!exports.ext_alloc) !== (!!exports.ext_free)) {
        return WokiExt.setError(handle, 'ext_alloc and ext_free must be exported as a pair');
      }
      if (exports.memory.buffer.byteLength > WokiExtContract.limits.memoryPages * 65536) {
        return WokiExt.setError(handle, 'extension memory exceeds the 32 MiB host cap');
      }
      WokiExt.instances.set(handle, record);
      WokiExt.clearError(handle);
      return 0;
    } catch (error) {
      return WokiExt.setError(handle, error.message);
    }
  },

  woki_web_ext_api_version__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_api_version: function(idPtr) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    try {
      if (!record) return WokiExt.setError(id, `extension ${id} is not loaded`);
      const version = record.instance.exports.ext_api_version();
      if (version <= 0) WokiExt.setError(record, `ext_api_version returned ${version}`);
      return version;
    } catch (error) {
      WokiExt.setError(id, error.message);
      return -1;
    }
  },

  woki_web_ext_init__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_init: function(idPtr) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    try {
      if (!record) return WokiExt.setError(id, `extension ${id} is not loaded`);
      const status = record.instance.exports.ext_init();
      if (status !== WokiExtContract.status.ok) WokiExt.setError(record, `ext_init returned ${status}`, status);
      return status;
    } catch (error) {
      return WokiExt.setError(id, error.message);
    }
  },

  woki_web_ext_tick__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_tick: function(idPtr, deltaMs) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    try {
      if (!record) return WokiExt.setError(id, `extension ${id} is not loaded`);
      record.instance.exports.ext_on_tick(deltaMs);
      return 0;
    } catch (error) {
      return WokiExt.setError(id, error.message);
    }
  },

  woki_web_ext_event__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_event: function(idPtr, eventType, payloadPtr, payloadLen) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    let guestPtr = 0;
    let primaryFailed = false;
    try {
      if (!record) return WokiExt.setError(id, `extension ${id} is not loaded`);
      if (payloadLen > WokiExtContract.limits.event) return WokiExt.setError(record, 'event payload exceeds 64 KiB limit');
      const exports = record.instance.exports;
      if (payloadLen > 0) {
        if (!exports.ext_alloc) {
          return WokiExt.setError(record, 'missing ext_alloc for event payload delivery');
        }
        guestPtr = exports.ext_alloc(payloadLen);
        if (guestPtr === 0) {
          return WokiExt.setError(record, 'ext_alloc returned null');
        }
        WokiExt.bytes(record, guestPtr, payloadLen).set(WokiExt.hostBytes(payloadPtr, payloadLen));
      }
      exports.ext_on_event(eventType, guestPtr, payloadLen);
      return 0;
    } catch (error) {
      primaryFailed = true;
      return WokiExt.setError(record || id, error.message);
    } finally {
      if (guestPtr !== 0 && record && record.instance.exports.ext_free) {
        try {
          record.instance.exports.ext_free(guestPtr, payloadLen);
        } catch (error) {
          if (!primaryFailed) return WokiExt.setError(record, `ext_free failed: ${error.message}`);
        }
      }
    }
  },

  woki_web_ext_event_named__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_event_named: function(idPtr, namePtr, nameLen, payloadPtr, payloadLen) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    let nameGuestPtr = 0;
    let payloadGuestPtr = 0;
    let primaryFailed = false;
    try {
      if (!record) return WokiExt.setError(id, `extension ${id} is not loaded`);
      if (nameLen === 0 || nameLen > WokiExtContract.limits.eventTopic || payloadLen > WokiExtContract.limits.event) {
        return WokiExt.setError(record, 'named event exceeds ABI limits');
      }
      const exports = record.instance.exports;
      if (!exports.ext_on_event_named) return 0;
      if (!exports.ext_alloc) return WokiExt.setError(record, 'missing ext_alloc for named event delivery');
      nameGuestPtr = exports.ext_alloc(nameLen);
      if (nameGuestPtr === 0) return WokiExt.setError(record, 'ext_alloc returned null for event topic');
      WokiExt.bytes(record, nameGuestPtr, nameLen).set(WokiExt.hostBytes(namePtr, nameLen));
      if (payloadLen > 0) {
        payloadGuestPtr = exports.ext_alloc(payloadLen);
        if (payloadGuestPtr === 0) return WokiExt.setError(record, 'ext_alloc returned null for event payload');
        WokiExt.bytes(record, payloadGuestPtr, payloadLen).set(WokiExt.hostBytes(payloadPtr, payloadLen));
      }
      exports.ext_on_event_named(nameGuestPtr, nameLen, payloadGuestPtr, payloadLen);
      return 0;
    } catch (error) {
      primaryFailed = true;
      return WokiExt.setError(record || id, error.message);
    } finally {
      const exports = record && record.instance.exports;
      if (exports && exports.ext_free) {
        try {
          if (payloadGuestPtr !== 0) exports.ext_free(payloadGuestPtr, payloadLen);
          if (nameGuestPtr !== 0) exports.ext_free(nameGuestPtr, nameLen);
        } catch (error) {
          if (!primaryFailed) return WokiExt.setError(record, `ext_free failed: ${error.message}`);
        }
      }
    }
  },

  woki_web_ext_command__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_command: function(idPtr, commandIdPtr, payloadPtr, payloadLen) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    let commandGuestPtr = 0;
    let commandLength = 0;
    let payloadGuestPtr = 0;
    let primaryFailed = false;
    try {
      if (!record) { WokiExt.setError(id, `extension ${id} is not loaded`); return -2147483648; }
      if (payloadLen > WokiExtContract.limits.event) { WokiExt.setError(record, 'command payload exceeds 64 KiB limit'); return -2147483648; }
      const exports = record.instance.exports;
      if (!exports.ext_on_command) {
        WokiExt.setError(record, `extension ${id} does not export ext_on_command`); return -2147483648;
      }
      if (!exports.ext_alloc) {
        WokiExt.setError(record, `extension ${id} does not export ext_alloc`); return -2147483648;
      }

      const commandBytes = new TextEncoder().encode(UTF8ToString(commandIdPtr));
      if (commandBytes.length === 0 || commandBytes.length > WokiExtContract.limits.path) {
        WokiExt.setError(record, 'command id exceeds ABI size limit'); return -2147483648;
      }
      commandLength = commandBytes.length;
      commandGuestPtr = exports.ext_alloc(commandLength);
      if (commandGuestPtr === 0) {
        WokiExt.setError(record, 'ext_alloc returned null for command id'); return -2147483648;
      }
      WokiExt.bytes(record, commandGuestPtr, commandBytes.length).set(commandBytes);

      if (payloadLen > 0) {
        payloadGuestPtr = exports.ext_alloc(payloadLen);
        if (payloadGuestPtr === 0) {
          WokiExt.setError(record, 'ext_alloc returned null for command payload'); return -2147483648;
        }
        WokiExt.bytes(record, payloadGuestPtr, payloadLen)
          .set(WokiExt.hostBytes(payloadPtr, payloadLen));
      }

      const result = exports.ext_on_command(
        commandGuestPtr, commandBytes.length, payloadGuestPtr, payloadLen);

      if (result !== WokiExtContract.status.ok) {
        primaryFailed = true;
        WokiExt.setError(record, `ext_on_command returned ${result}`, result);
      }
      return result;
    } catch (error) {
      primaryFailed = true;
      WokiExt.setError(record || id, error.message);
      return -2147483648;
    } finally {
      if (record && record.instance.exports.ext_free) {
        let cleanupError = null;
        for (const [ptr, len] of [[payloadGuestPtr, payloadLen], [commandGuestPtr, commandLength]]) {
          if (ptr === 0) continue;
          try {
            record.instance.exports.ext_free(ptr, len);
          } catch (error) {
            cleanupError = cleanupError || error;
          }
        }
        if (cleanupError && !primaryFailed) {
          WokiExt.setError(record, `ext_free failed: ${cleanupError.message}`);
          return -2147483648;
        }
      }
    }
  },

  woki_web_ext_unload__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_unload: function(idPtr) {
    const id = UTF8ToString(idPtr);
    const record = WokiExt.instances.get(id);
    if (!record) return;
    try {
      record.instance.exports.ext_on_unload();
    } catch (error) {
      WokiExt.setError(record, `ext_on_unload failed: ${error.message}`);
    }
    WokiExt.instances.delete(id);
    WokiExt.errors.delete(id);
  },

  woki_web_ext_discard__deps: ['$WokiExt', '$UTF8ToString'],
  woki_web_ext_discard: function(idPtr) {
    const id = UTF8ToString(idPtr);
    WokiExt.instances.delete(id);
    WokiExt.errors.delete(id);
  },

  woki_web_ext_last_error__deps: ['$WokiExt', '$UTF8ToString', '$stringToUTF8'],
  woki_web_ext_last_error: function(idPtr, outPtr, outCap) {
    if (!outPtr || outCap <= 0) return -1;
    stringToUTF8(WokiExt.errors.get(UTF8ToString(idPtr)) || '', outPtr, outCap);
    return 0;
  },
});
