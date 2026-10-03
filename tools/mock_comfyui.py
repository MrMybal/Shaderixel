"""Minimal stand-in for a ComfyUI server, to test Shaderixel's ComfyUI client.

Implements /upload/image, /prompt, /history/<id> and /view like ComfyUI. The
"generated" image is the uploaded input with inverted colors (image-to-image)
or a gradient of the requested size (text-to-image), so a test can tell which
path ran. Everything received is printed.

    python tools/mock_comfyui.py [port]        (default 8188)
"""
import io
import json
import sys
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from PIL import Image, ImageOps

STATE = {"input": None, "jobs": {}, "polls": {}}


def make_output(job):
    if job.get("image") and STATE["input"] is not None:
        img = ImageOps.invert(STATE["input"].convert("RGB"))
    else:
        w, h = job.get("size", (512, 512))
        img = Image.new("RGB", (w, h))
        px = img.load()
        for y in range(h):
            for x in range(w):
                px[x, y] = (x * 255 // max(w - 1, 1), y * 255 // max(h - 1, 1), 160)
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def reply(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if self.path == "/upload/image":
            start = body.find(b"\x89PNG")
            end = body.rfind(b"IEND") + 8
            STATE["input"] = Image.open(io.BytesIO(body[start:end]))
            print(f"[mock] upload {STATE['input'].size}", flush=True)
            return self.reply(200, {"name": "shaderixel_input.png", "subfolder": "", "type": "input"})
        if self.path == "/prompt":
            wf = json.loads(body)["prompt"]
            job = {}
            errors = {}
            for nid, n in wf.items():
                ct, inp = n["class_type"], n["inputs"]
                if ct == "CheckpointLoaderSimple" and not inp.get("ckpt_name"):
                    errors[nid] = {"class_type": ct, "errors": [{"message": "Value not in list", "details": "ckpt_name"}]}
                if ct.startswith("CLIPTextEncode"):
                    job.setdefault("texts", []).append(inp.get("text"))
                if ct == "KSampler":
                    job["seed"], job["denoise"] = inp.get("seed"), inp.get("denoise")
                if ct == "EmptyLatentImage":
                    job["size"] = (inp["width"], inp["height"])
                if ct == "LoadImage":
                    job["image"] = inp["image"]
            if not any(n["class_type"] == "SaveImage" for n in wf.values()):
                return self.reply(400, {"error": {"type": "prompt_outputs_failed_validation", "message": "Prompt has no outputs"}, "node_errors": {}})
            if errors:
                return self.reply(400, {"error": {"message": "Prompt outputs failed validation"}, "node_errors": errors})
            pid = str(uuid.uuid4())
            STATE["jobs"][pid] = job
            STATE["polls"][pid] = 0
            print(f"[mock] prompt {json.dumps(job, ensure_ascii=False)}", flush=True)
            return self.reply(200, {"prompt_id": pid, "number": 1, "node_errors": {}})
        self.reply(404, {"error": "not found"})

    def do_GET(self):
        u = urlparse(self.path)
        if u.path.startswith("/history/"):
            pid = u.path.split("/")[-1]
            if pid not in STATE["jobs"]:
                return self.reply(200, {})
            STATE["polls"][pid] += 1
            if STATE["polls"][pid] < 2:  # still running on the first poll
                return self.reply(200, {})
            out = {"9": {"images": [{"filename": f"Shaderixel_{pid[:8]}.png", "subfolder": "", "type": "output"}]}}
            return self.reply(200, {pid: {"outputs": out, "status": {"status_str": "success", "completed": True, "messages": []}}})
        if u.path == "/view":
            q = parse_qs(u.query)
            name = q.get("filename", [""])[0]
            pid = next((p for p in STATE["jobs"] if name == f"Shaderixel_{p[:8]}.png"), None)
            if pid is None:
                return self.reply(404, {"error": "no such file"})
            print(f"[mock] view {name}", flush=True)
            return self.reply(200, make_output(STATE["jobs"][pid]), "image/png")
        self.reply(404, {"error": "not found"})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8188
    print(f"[mock] ComfyUI stand-in on http://127.0.0.1:{port}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
