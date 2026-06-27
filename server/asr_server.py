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
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from zhconv import convert
from config import DEEPSEEK_API_KEY
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

_upload_count = 0

def _deepseek_correct(text):
    """DeepSeek AI 纠错：修正同音字/拼写、语法，失败时返回原文"""
    if not text or len(text) < 2:
        return text
    try:
        import requests
        resp = requests.post(
            "https://api.deepseek.com/v1/chat/completions",
            headers={
                "Authorization": f"Bearer {DEEPSEEK_API_KEY}",
                "Content-Type": "application/json"
            },
            json={
                "model": "deepseek-chat",
                "messages": [{
                    "role": "system",
                    "content": (
                        "你是语音识别纠错助手。根据输入语言自动选择纠错策略：\n"
                        "- 中文：修正错别字、语法、同音词，使文本通顺\n"
                        "- 英文：修正 spelling、grammar、homophones\n"
                        "- 中英混合：分别按各自规则修正，保持原语言不变\n"
                        "严格只输出纠错后的纯文本，不加任何解释、前缀或后缀。"
                    )
                }, {
                    "role": "user", "content": text
                }],
                "temperature": 0.3,
                "max_tokens": 500
            },
            timeout=10
        )
        if resp.status_code == 200:
            corrected = resp.json()["choices"][0]["message"]["content"].strip()
            print(f"[ASR]   DeepSeek: '{text}' -> '{corrected}'")
            return corrected
        else:
            print(f"[ASR]   DeepSeek HTTP {resp.status_code}: {resp.text[:100]}")
    except Exception as e:
        print(f"[ASR]   DeepSeek error: {e}")
    return text

def _whisper_recognize(audio_bytes):
    global _upload_count
    _upload_count += 1
    try:
        # 检查 WAV 头
        if len(audio_bytes) >= 44:
            import struct
            riff = audio_bytes[0:4]
            fmt = audio_bytes[8:12]
            channels = struct.unpack_from('<H', audio_bytes, 22)[0]
            sr = struct.unpack_from('<I', audio_bytes, 24)[0]
            bits = struct.unpack_from('<H', audio_bytes, 34)[0]
            data_size = struct.unpack_from('<I', audio_bytes, 40)[0]
            print(f"[ASR]   Header: {riff} {fmt} ch={channels} sr={sr} bits={bits} data={data_size}B")
            if riff != b'RIFF':
                print(f"[ASR]   WARNING: Not a valid WAV file!")
        else:
            print(f"[ASR]   WARNING: File too small, no valid header")

        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
            tmp.write(audio_bytes)
            tmp_path = tmp.name
        segments, info = WHISPER_MODEL.transcribe(tmp_path, language=None,
                                                   beam_size=5, vad_filter=True)
        # 仅允许中/英文：检测到其他语言则回退用中文重识别
        if info.language not in ("zh", "en"):
            print(f"[ASR]   Detected '{info.language}', falling back to en")
            segments, info = WHISPER_MODEL.transcribe(tmp_path, language="en",
                                                       beam_size=5, vad_filter=True)
        text = "".join(seg.text for seg in segments).strip()
        text = convert(text, "zh-cn")
        text = _deepseek_correct(text)
        print(f"[ASR]   Result: '{text}' (lang={info.language}, prob={info.language_probability:.2f})")

        # 保存 debug 音频到专用文件夹
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        os.makedirs("debug_wav", exist_ok=True)
        save_path = f"debug_wav/{ts}_{_upload_count:03d}_{info.language}_{len(audio_bytes)}B.wav"
        with open(save_path, "wb") as sf:
            sf.write(audio_bytes)
        print(f"[ASR]   Saved debug WAV to {save_path}")

        os.unlink(tmp_path)
        return text, None
    except Exception as e:
        print(f"[ASR]   ERROR: {e}")
        import traceback; traceback.print_exc()
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
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        print(f"[{self.client_address[0]}] {args[0]}")

    def do_GET(self):
        page = TEST_PAGE.replace("__MODE__", MODE)
        self._respond(200, "text/html; charset=utf-8", page.encode())

    def do_POST(self):
        path = urlparse(self.path).path

        # /asr — 语音识别
        if path == "/asr":
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
                          json.dumps(resp, ensure_ascii=False, separators=(',', ':')).encode())
            return

        # /chat — AI 对话 + TTS
        if path == "/chat":
            length = int(self.headers.get("Content-Length", 0))
            if length == 0:
                self._respond(400, "application/json",
                              json.dumps({"reply":"","err":"Empty body"}).encode())
                return
            body = self.rfile.read(length).decode("utf-8")
            try:
                req = json.loads(body)
            except Exception:
                self._respond(400, "application/json",
                              json.dumps({"reply":"","err":"Invalid JSON"}).encode())
                return
            user_text = req.get("text", "")
            if not user_text or len(user_text) < 1:
                self._respond(400, "application/json",
                              json.dumps({"reply":"","err":"Empty text"}).encode())
                return
            print(f"[Chat] '{user_text}'")
            reply, audio_b64 = _chat_reply(user_text)
            resp = {"reply": reply, "audio_b64": audio_b64 or ""}
            self._respond(200, "application/json; charset=utf-8",
                          json.dumps(resp, ensure_ascii=False).encode())
            return

        self._respond(404, "text/plain", b"Not Found")

    def _respond(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()

def _chat_reply(text):
    """DeepSeek AI 对话 + edge-tts 语音合成，返回 (reply, audio_b64_or_None)"""
    reply = text  # 默认返回原文
    audio_b64 = None
    try:
        import requests
        resp = requests.post(
            "https://api.deepseek.com/v1/chat/completions",
            headers={
                "Authorization": f"Bearer {DEEPSEEK_API_KEY}",
                "Content-Type": "application/json"
            },
            json={
                "model": "deepseek-chat",
                "messages": [{
                    "role": "system",
                    "content": "你是box-demo语音助手。用简洁自然的语言回答，一般不超过3句话。"
                }, {
                    "role": "user", "content": text
                }],
                "temperature": 0.7,
                "max_tokens": 300
            },
            timeout=15
        )
        if resp.status_code == 200:
            reply = resp.json()["choices"][0]["message"]["content"].strip()
            print(f"[Chat] Reply: '{reply}'")
        else:
            print(f"[Chat] DeepSeek HTTP {resp.status_code}: {resp.text[:100]}")
    except Exception as e:
        print(f"[Chat] DeepSeek error: {e}")

    # 尝试 TTS
    try:
        import base64, asyncio
        audio_b64 = asyncio.run(_tts(reply))
        print(f"[Chat] TTS: {len(audio_b64)} chars base64")
    except Exception as e:
        print(f"[Chat] TTS failed: {e}")

    return reply, audio_b64


async def _tts(text):
    """edge-tts 语音合成 → MP3 → WAV 转换，返回 base64 编码的 WAV"""
    import edge_tts, base64, io, tempfile, os
    communicate = edge_tts.Communicate(text, "zh-CN-XiaoxiaoNeural")
    tmp_mp3 = os.path.join(tempfile.gettempdir(), f"_tts_{os.getpid()}.mp3")
    await communicate.save(tmp_mp3)
    try:
        from pydub import AudioSegment
        seg = AudioSegment.from_mp3(tmp_mp3)
        seg = seg.set_frame_rate(22050).set_channels(1).set_sample_width(2)
        wav_io = io.BytesIO()
        seg.export(wav_io, format="wav")
        wav_data = wav_io.getvalue()
    except ImportError:
        print("[Chat] TTS: pydub not installed, returning raw MP3")
        with open(tmp_mp3, "rb") as f:
            wav_data = f.read()
    os.remove(tmp_mp3)
    return base64.b64encode(wav_data).decode()

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
