#!/usr/bin/env python3
"""
ASR Server for box-demo ESP32-S3
================================
POST /asr   → 接收 WAV 音频，返回识别文本 (JSON)
GET  /      → 网页测试界面（可上传文件手动测试）

启动: python asr_server.py
自动检测: Mock 模式 / Whisper-CPU / Whisper-CUDA
"""

import os, sys, json, io, tempfile, traceback
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

# ==================== 模式检测 ====================

WHISPER_MODEL = None
MODE = "Mock"

def _init_whisper():
    global WHISPER_MODEL, MODE
    try:
        from faster_whisper import WhisperModel
        model_size = "base"
        device = "cuda" if _has_cuda() else "cpu"
        compute = "float16" if device == "cuda" else "int8"
        WHISPER_MODEL = WhisperModel(model_size, device=device, compute_type=compute,
                                     download_root="./whisper_models")
        MODE = f"Whisper-{model_size} ({device})"
        print(f"[ASR] {MODE} loaded OK")
        return True
    except ImportError:
        print("[ASR] faster-whisper not installed, using Mock mode")
        print("[ASR] Install: pip install faster-whisper")
        return False
    except Exception as e:
        print(f"[ASR] Whisper init failed: {e}, using Mock mode")
        return False

def _has_cuda():
    try:
        import torch
        return torch.cuda.is_available()
    except Exception:
        return False

# ==================== 识别 ====================

def recognize(audio_bytes):
    """返回 (text, error)"""
    if WHISPER_MODEL:
        return _whisper_recognize(audio_bytes)
    return _mock_recognize()

def _whisper_recognize(audio_bytes):
    try:
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
            tmp.write(audio_bytes)
            tmp_path = tmp.name
        segments, info = WHISPER_MODEL.transcribe(tmp_path, language="zh",
                                                   beam_size=5, vad_filter=True)
        text = "".join(seg.text for seg in segments).strip()
        os.unlink(tmp_path)
        return text, None
    except Exception as e:
        return "", str(e)

_mock_count = 0
def _mock_recognize():
    global _mock_count
    _mock_count += 1
    samples = [
        "你好世界",
        "今天天气不错",
        "这是一段语音识别测试",
        "欢迎使用 box-demo 开发板",
        "语音识别功能正常工作",
    ]
    return samples[_mock_count % len(samples)], None

# ==================== HTML 测试页面 ====================

TEST_PAGE = r"""<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ASR Test Server</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;max-width:640px;margin:20px auto;padding:0 16px;background:#1a1a2e;color:#eee}
h1{color:#00d4ff;margin-bottom:4px}
.mode{font-size:13px;color:#888;margin-bottom:16px}
.card{background:#16213e;border-radius:12px;padding:20px;margin-bottom:16px}
.card h2{font-size:16px;color:#00d4ff;margin-bottom:12px}
.row{display:flex;align-items:center;gap:12px;margin-bottom:10px}
.rec-btn{width:64px;height:64px;border-radius:50%;border:4px solid #ff4444;background:#1a1a2e;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:0.2s}
.rec-btn:hover{background:#2a1a1a}
.rec-btn.recording{background:#ff4444;border-color:#ff6666;animation:pulse 1.5s infinite}
.rec-btn.recording .dot{border-radius:4px;width:20px;height:20px;background:#fff}
.rec-btn .dot{border-radius:50%;width:24px;height:24px;background:#ff4444}
@keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(255,68,68,0.4)}50%{box-shadow:0 0 0 16px rgba(255,68,68,0)}}
.timer{font-size:24px;font-weight:600;color:#ff4444;min-width:50px}
.status{font-size:14px;color:#888}
button{background:#00d4ff;color:#1a1a2e;border:none;padding:10px 24px;border-radius:8px;font-size:15px;cursor:pointer;font-weight:600}
button:hover{background:#00b8e6}
button:disabled{background:#555;cursor:not-allowed}
input[type=file]{margin-bottom:10px;color:#aaa}
.result{background:#0f3460;border-radius:8px;padding:14px;min-height:50px;margin-top:12px;font-size:18px;line-height:1.6;white-space:pre-wrap;word-break:break-all}
.result:empty::after{content:"(waiting...)";color:#555}
.divider{height:1px;background:#333;margin:16px 0}
.api{font-size:13px;color:#888;margin-top:8px;font-family:monospace}
</style>
</head>
<body>
<h1>box-demo ASR Server</h1>
<div class="mode">Mode: __MODE__</div>

<!-- ====== PC 麦克风测试 ====== -->
<div class="card">
  <h2>&#x1F3A4; PC Microphone Test</h2>
  <div class="row">
    <div class="rec-btn" id="recBtn" onclick="toggleRecord()">
      <div class="dot"></div>
    </div>
    <div>
      <div class="timer" id="timer">0s</div>
      <div class="status" id="recStatus">Click to record</div>
    </div>
  </div>
  <div class="result" id="micResult"></div>
</div>

<!-- ====== 文件上传 ====== -->
<div class="card">
  <h2>&#x1F4C1; File Upload</h2>
  <input type="file" id="file" accept=".wav,audio/wav"><br>
  <button onclick="doFileRecognize()">Recognize File</button>
  <div class="result" id="fileResult"></div>
</div>

<!-- ====== ESP32 API 说明 ====== -->
<div class="card">
  <h2>&#x1F4E1; ESP32 API</h2>
  <div class="api">POST /asr<br>Content-Type: audio/wav<br>Body: WAV binary<br>Response: {"text":"...","err":0}</div>
</div>

<script>
// ===== PCM → WAV 编码 =====
function encodeWAV(samples, sampleRate) {
  const bitsPerSample = 16;
  const byteRate = sampleRate * 2;
  const dataSize = samples.length * 2;
  const buf = new ArrayBuffer(44 + dataSize);
  const v = new DataView(buf);
  function w(s,o,l){for(let i=0;i<l;i++) v.setUint8(o+i,s.charCodeAt(i))}
  w("RIFF",0,4); v.setUint32(4,36+dataSize,true); w("WAVE",8,4);
  w("fmt ",12,4); v.setUint32(16,16,true); v.setUint16(20,1,true);
  v.setUint16(22,1,true); v.setUint32(24,sampleRate,true);
  v.setUint32(28,byteRate,true); v.setUint16(32,2,true);
  v.setUint16(34,bitsPerSample,true); w("data",36,4);
  v.setUint32(40,dataSize,true);
  const out = new Int16Array(buf,44,samples.length);
  for(let i=0;i<samples.length;i++) out[i]=Math.max(-32768,Math.min(32767,samples[i]*32768));
  return new Blob([buf],{type:"audio/wav"});
}

// ===== 麦克风录音 =====
let audioCtx=null, recNode=null, recSamples=[], recStart=0, recTimer=null;
const SAMPLE_RATE = 22050;

async function toggleRecord() {
  const btn = document.getElementById("recBtn");
  if (btn.classList.contains("recording")) { stopRecord(); return; }

  // 开始录音
  try {
    const stream = await navigator.mediaDevices.getUserMedia({audio:{sampleRate:44100,channelCount:1}});
    audioCtx = new AudioContext({sampleRate:SAMPLE_RATE});
    const source = audioCtx.createMediaStreamSource(stream);
    recNode = audioCtx.createScriptProcessor(4096,1,1);
    recSamples = [];

    recNode.onaudioprocess = function(e) {
      const ch = e.inputBuffer.getChannelData(0);
      for(let i=0;i<ch.length;i++) recSamples.push(ch[i]);
    };

    source.connect(recNode);
    recNode.connect(audioCtx.destination);

    btn.classList.add("recording");
    document.getElementById("recStatus").textContent = "Recording...";
    document.getElementById("micResult").textContent = "";
    recStart = Date.now();
    recTimer = setInterval(()=>{
      document.getElementById("timer").textContent = Math.round((Date.now()-recStart)/1000)+"s";
    },200);
  } catch(e) {
    document.getElementById("recStatus").textContent = "Mic error: "+e.message;
  }
}

function stopRecord() {
  const btn = document.getElementById("recBtn");
  btn.classList.remove("recording");
  document.getElementById("recStatus").textContent = "Recognizing...";
  clearInterval(recTimer);

  if (recNode) { recNode.disconnect(); recNode = null; }
  if (audioCtx) { audioCtx.close(); audioCtx = null; }

  const wav = encodeWAV(recSamples, SAMPLE_RATE);
  sendToASR(wav, "micResult", "recStatus");
}

// ===== 文件识别 =====
function doFileRecognize() {
  const f = document.getElementById("file").files[0];
  if (!f) { document.getElementById("fileResult").textContent = "Please select a WAV file"; return; }
  document.getElementById("fileResult").textContent = "Recognizing...";
  sendToASR(f, "fileResult", null);
}

// ===== 发送到服务器 =====
async function sendToASR(blob, resultId, statusId) {
  try {
    const r = await fetch("/asr", {method:"POST",body:blob});
    const j = await r.json();
    document.getElementById(resultId).textContent = j.text || "(empty)";
    if (j.err) document.getElementById(resultId).textContent += "\n[Error: "+j.err+"]";
    if (statusId) document.getElementById(statusId).textContent = "Done";
  } catch(e) {
    document.getElementById(resultId).textContent = "Error: "+e.message;
    if (statusId) document.getElementById(statusId).textContent = "Failed";
  }
}
</script>
</body>
</html>"""

# ==================== HTTP Server ====================

class ASRHandler(BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        print(f"[{self.client_address[0]}] {args[0]}")

    def do_GET(self):
        page = TEST_PAGE.replace("__MODE__", MODE)
        self._respond(200, "text/html; charset=utf-8", page.encode())

    def do_POST(self):
        path = urlparse(self.path).path
        if path != "/asr":
            self._respond(404, "text/plain", b"Not Found")
            return

        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            self._respond(400, "application/json",
                          json.dumps({"text":"","err":"Empty body"}).encode())
            return

        audio = self.rfile.read(length)
        print(f"[ASR] Received {length} bytes from {self.client_address[0]}")

        text, error = recognize(audio)
        resp = {"text": text, "err": 0 if not error else error}
        self._respond(200, "application/json; charset=utf-8",
                      json.dumps(resp, ensure_ascii=False).encode())

    def _respond(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

# ==================== 主入口 ====================

def main():
    global MODE
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080

    _init_whisper()

    import socket
    ip = socket.gethostbyname(socket.gethostname())
    if ip.startswith("127."):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
        except Exception:
            ip = "localhost"

    print(f"\n{'='*50}")
    print(f"  ASR Server Ready")
    print(f"  Mode:    {MODE}")
    print(f"  Web UI:  http://{ip}:{port}")
    print(f"  API:     POST http://{ip}:{port}/asr")
    print(f"{'='*50}\n")

    server = HTTPServer(("0.0.0.0", port), ASRHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[ASR] Shutting down...")
        server.shutdown()

if __name__ == "__main__":
    main()
