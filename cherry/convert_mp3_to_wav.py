"""Convert audio/music.mp3 to 16-bit 44.1kHz stereo WAV for ESP32 I2S playback."""
from pydub import AudioSegment
import os

src = os.path.join(os.path.dirname(__file__), "..", "audio", "music.mp3")
dst = os.path.join(os.path.dirname(__file__), "..", "audio", "music.wav")

audio = AudioSegment.from_file(src, format="mp3")
audio = audio.set_frame_rate(44100).set_sample_width(2).set_channels(2)
audio.export(dst, format="wav")
print(f"OK: {len(audio)/1000:.1f}s {audio.frame_rate}Hz {audio.channels}ch -> {dst}")
