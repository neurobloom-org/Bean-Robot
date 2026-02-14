import os
import asyncio
import struct
from flask import Flask, request, jsonify, send_file
import google.generativeai as genai
import edge_tts

# --- CONFIG ---
os.environ["AIzaSyD52V5oYThjh4vhxbts1jolQv6T-uEfYwA"] = "AIzaSyD52V5oYThjh4vhxbts1jolQv6T-uEfYwA" # <--- PASTE KEY HERE!
genai.configure(api_key=os.environ["AIzaSyD52V5oYThjh4vhxbts1jolQv6T-uEfYwA"])

app = Flask(__name__)
model = genai.GenerativeModel("gemini-1.5-flash")

# Global filename for the latest reply
REPLY_FILE = "reply.mp3"

# CHANGE THIS LINE in server.py
def add_wav_header(pcm_data, sample_rate=16000, channels=1, bit_depth=16):
    header = struct.pack('<4sI4s4sIHHIIHH4sI',
        b'RIFF', 36 + len(pcm_data), b'WAVE', b'fmt ', 16, 1, channels,
        sample_rate, sample_rate * channels * (bit_depth // 8),
        channels * (bit_depth // 8), bit_depth, b'data', len(pcm_data)
    )
    return header + pcm_data

# --- ROUTE 1: RECEIVE AUDIO ---
@app.route('/chat', methods=['POST'])
def chat():
    print("📩 Audio received...")
    wav_data = add_wav_header(request.data)
    
    with open("input.wav", "wb") as f:
        f.write(wav_data)

    # FIND THIS BLOCK IN YOUR SERVER.PY
    print("🧠 Gemini Thinking...")
    try:
        myfile = genai.upload_file("input.wav")
        response = model.generate_content([
            myfile, 
            "You are Bean. Reply in one short, empathetic sentence."
        ])
        reply_text = response.text
        
    except Exception as e:  # <--- CHANGE THIS LINE
        print(f"❌ GEMINI ERROR: {e}")  # <--- AND THIS LINE
        reply_text = "I am listening."
    
    print(f"🤖 Bean says: {reply_text}")

    # Generate MP3 and save it
    asyncio.run(generate_audio(reply_text, REPLY_FILE))
    
    # Tell ESP32 we are ready
    return "READY"

# --- ROUTE 2: SERVE AUDIO ---
@app.route('/reply.mp3', methods=['GET'])
def get_audio():
    print("📤 Sending MP3 to Bean...")
    return send_file(REPLY_FILE, mimetype="audio/mpeg")

async def generate_audio(text, filename):
    communicate = edge_tts.Communicate(text, "en-US-AnaNeural")
    await communicate.save(filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)