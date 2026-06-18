#!/usr/bin/env python3
"""EvolutionWar sprite generator using OpenAI gpt-image-1.

Reads the API key from Content/keys/open ai api key.txt (the sk- token on any line).
Supports reference-guided generation via the images/edits endpoint (pass --ref images)
or plain text-to-image via images/generations.

Examples:
  python Tools/gen_sprite.py --prompt "cyborg soldier sprite" --out Content/Art/Source/cyborg.png --transparent
  python Tools/gen_sprite.py --prompt "..." --ref Content/Reference/vibe.png --out out.png --transparent
"""
import argparse, base64, json, os, re, sys, urllib.request, urllib.error, mimetypes, uuid

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEY_FILE = os.path.join(PROJECT, "Content", "keys", "open ai api key.txt")
API_BASE = "https://api.openai.com/v1"


def get_key():
    env = os.environ.get("OPENAI_API_KEY")
    if env and env.startswith("sk-"):
        return env.strip()
    with open(KEY_FILE, "r", encoding="utf-8", errors="replace") as f:
        m = re.search(r"sk-[A-Za-z0-9_\-]+", f.read())
    if not m:
        sys.exit("No sk- key found in key file")
    return m.group(0)


def _multipart(fields, files):
    boundary = "----EWBoundary" + uuid.uuid4().hex
    body = bytearray()
    for k, v in fields:
        body += f"--{boundary}\r\n".encode()
        body += f'Content-Disposition: form-data; name="{k}"\r\n\r\n'.encode()
        body += f"{v}\r\n".encode()
    for k, path in files:
        fn = os.path.basename(path)
        ctype = mimetypes.guess_type(fn)[0] or "image/png"
        with open(path, "rb") as fh:
            data = fh.read()
        body += f"--{boundary}\r\n".encode()
        body += f'Content-Disposition: form-data; name="{k}"; filename="{fn}"\r\n'.encode()
        body += f"Content-Type: {ctype}\r\n\r\n".encode()
        body += data + b"\r\n"
    body += f"--{boundary}--\r\n".encode()
    return body, f"multipart/form-data; boundary={boundary}"


def _post(url, headers, data):
    req = urllib.request.Request(url, data=data, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=600) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        sys.exit(f"HTTP {e.code}: {e.read().decode()[:1000]}")


def generate(prompt, out, size, refs, transparent, quality):
    key = get_key()
    auth = {"Authorization": f"Bearer {key}"}
    if refs:
        fields = [("model", "gpt-image-1"), ("prompt", prompt), ("size", size),
                  ("quality", quality), ("n", "1")]
        if transparent:
            fields += [("background", "transparent")]
        files = [("image[]", p) for p in refs]
        body, ctype = _multipart(fields, files)
        headers = dict(auth); headers["Content-Type"] = ctype
        resp = _post(f"{API_BASE}/images/edits", headers, body)
    else:
        payload = {"model": "gpt-image-1", "prompt": prompt, "size": size,
                   "quality": quality, "n": 1, "output_format": "png"}
        if transparent:
            payload["background"] = "transparent"
        headers = dict(auth); headers["Content-Type"] = "application/json"
        resp = _post(f"{API_BASE}/images/generations", headers, json.dumps(payload).encode())
    b64 = resp["data"][0]["b64_json"]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        f.write(base64.b64decode(b64))
    print("WROTE", out, os.path.getsize(out), "bytes")
    if "usage" in resp:
        print("USAGE", json.dumps(resp["usage"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", default="1024x1024",
                    choices=["1024x1024", "1024x1536", "1536x1024", "auto"])
    ap.add_argument("--ref", nargs="*", default=[], help="reference image paths")
    ap.add_argument("--transparent", action="store_true")
    ap.add_argument("--quality", default="high", choices=["low", "medium", "high", "auto"])
    a = ap.parse_args()
    generate(a.prompt, a.out, a.size, a.ref, a.transparent, a.quality)


if __name__ == "__main__":
    main()
