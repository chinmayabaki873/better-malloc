import ctypes
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
import time
from urllib.parse import parse_qs, urlparse

# Load compiled C shared library
lib_path = os.path.abspath("./libmymalloc.so")
lib = ctypes.CDLL(lib_path)

# C function signatures
lib.Mem_Init.argtypes = [ctypes.c_size_t]
lib.Mem_Init.restype = ctypes.c_int

lib.Mem_Alloc.argtypes = [ctypes.c_size_t]
lib.Mem_Alloc.restype = ctypes.c_void_p

lib.Mem_Free.argtypes = [ctypes.c_void_p]
lib.Mem_Free.restype = ctypes.c_int

lib.Mem_FreeByIndex.argtypes = [ctypes.c_int]
lib.Mem_FreeByIndex.restype = ctypes.c_int

lib.Mem_SetPolicy.argtypes = [ctypes.c_int]
lib.Mem_SetPolicy.restype = None

lib.Mem_Reset.argtypes = []
lib.Mem_Reset.restype = None

lib.Mem_Dump.argtypes = []
lib.Mem_Dump.restype = None

lib.Mem_ExportJSON.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
lib.Mem_ExportJSON.restype = ctypes.c_int

# Initialize 1 MB (1,048,576 bytes) via single mmap() inside C
HEAP_1MB = 1024 * 1024
lib.Mem_Init(HEAP_1MB)
print("=== Initialized 1 MB C Heap via libmymalloc.so ===")
lib.Mem_Dump()


def get_c_heap_json():
  buf = ctypes.create_string_buffer(65536)
  lib.Mem_ExportJSON(buf, 65536)
  return buf.value


class AllocatorHandler(BaseHTTPRequestHandler):

  def log_message(self, format, *args):
    pass

  def _send_json(self):
    data = get_c_heap_json()
    self.send_response(200)
    self.send_header("Content-Type", "application/json")
    self.send_header("Access-Control-Allow-Origin", "*")
    self.end_headers()
    self.wfile.write(data)

  def do_GET(self):
    parsed = urlparse(self.path)
    if parsed.path == "/" or parsed.path == "/visualizer.html":
      with open("visualizer.html", "rb") as f:
        content = f.read()
      self.send_response(200)
      self.send_header("Content-Type", "text/html")
      self.end_headers()
      self.wfile.write(content)
    elif parsed.path == "/api/state":
      self._send_json()
    else:
      self.send_error(404)

  def do_POST(self):
    parsed = urlparse(self.path)
    qs = parse_qs(parsed.query)

    if parsed.path == "/api/alloc":
      size = int(qs.get("size", [512])[0])
      ptr = lib.Mem_Alloc(size)
      print(f"\n[C Call] Mem_Alloc({size}) -> {hex(ptr) if ptr else 'NULL'}")
      lib.Mem_Dump()

    elif parsed.path == "/api/free":
      idx = int(qs.get("index", [0])[0])
      lib.Mem_FreeByIndex(idx)
      print(f"\n[C Call] Mem_FreeByIndex({idx})")
      lib.Mem_Dump()

    elif parsed.path == "/api/policy":
      pol = int(qs.get("id", [0])[0])
      lib.Mem_SetPolicy(pol)
      print(f"\n[C Call] Mem_SetPolicy({pol})")
      lib.Mem_Dump()

    elif parsed.path == "/api/reset":
      lib.Mem_Reset()
      print("\n[C Call] Mem_Reset()")
      lib.Mem_Dump()

    elif parsed.path == "/api/demo3holes":
      lib.Mem_Reset()
      lib.Mem_SetPolicy(0)
      for s in [256, 128, 512, 128, 128, 128]:
        lib.Mem_Alloc(s)
      lib.Mem_FreeByIndex(0)
      lib.Mem_FreeByIndex(2)
      lib.Mem_FreeByIndex(4)
      print("\n[C Call] Loaded 3-Hole Fragmentation Scenario")
      lib.Mem_Dump()

    elif parsed.path == "/api/benchmark":
      names = ["first fit", "best fit", "worst fit", "next fit"]
      notes = [
          "Fastest start-of-heap search",
          "Minimum fragmentation / saves tail",
          "Leaves large reusable splits",
          "Fast cyclic scan via rover",
      ]
      reqs = [
          64,
          128,
          32,
          256,
          48,
          512,
          96,
          16,
          128,
          64,
          200,
          80,
          40,
          160,
          300,
          72,
      ]
      results = []

      for p in range(4):
        lib.Mem_Reset()
        lib.Mem_SetPolicy(p)
        ptrs = [None] * len(reqs)
        total_ops = 0

        t0 = time.perf_counter()
        for _ in range(1000):
          for i, r in enumerate(reqs):
            ptrs[i] = lib.Mem_Alloc(r)
            total_ops += 1
          for i in range(0, len(reqs), 2):
            if ptrs[i]:
              lib.Mem_Free(ptrs[i])
              ptrs[i] = None
          for i in range(0, len(reqs), 2):
            ptrs[i] = lib.Mem_Alloc(reqs[len(reqs) - 1 - i])
            total_ops += 1
          lib.Mem_Reset()

        ms = (time.perf_counter() - t0) * 1000.0
        ops_sec = int(total_ops / (ms / 1000.0)) if ms > 0 else 0
        results.append({
            "policy": names[p],
            "ms": round(ms, 2),
            "ops": ops_sec,
            "note": notes[p],
        })

      lib.Mem_Reset()
      self.send_response(200)
      self.send_header("Content-Type", "application/json")
      self.end_headers()
      self.wfile.write(json.dumps({"benchmarks": results}).encode())
      return

    self._send_json()


if __name__ == "__main__":
  port = int(os.environ.get("PORT", 8080))
  print(f"\nLive C Visualizer running at: http://localhost:{port}")
  HTTPServer(("0.0.0.0", port), AllocatorHandler).serve_forever()