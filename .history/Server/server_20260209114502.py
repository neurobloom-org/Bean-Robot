import os
import asyncio
import struct
from flask import Flask, request, Response
import google.generativeai as genai
import edge_tts

# --- CONFIGURATION ---
# PASTE YOUR KEY HERE
os.environ["GOOGLE_API_KEY"] = "YOUR_GEMINI_API_KEY"
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

app = Flask(__name__)

# Setup Gemini Model (Flash is fast and supports audio)
model = genai.GenerativeModel("gemini-1.5-flash")

# --- HELPER: Add WAV Header (So Gemini can hear raw ESP32 audio) ---
def add_wav_header(pcm_data, sample_rate=8000, channels=1, bit_depth=16):
    header = struct.pack('<4sI4s4sIHHIIHH4sI',
        b'RIFF',
        36 + len(pcm_data),
        b'WAVE',
        b'fmt ',
        16,                # PCM format size
        1,                 # Audio format (1 = PCM)
        channels,
        sample_rate,
        sample_rate * channels * (bit_depth // 8),
        channels * (bit_depth // 8),
        bit_depth,
        b'data',
        len(pcm_data)
    )
    return header + pcm_data

@app.route('/chat', methods=['POST'])
def chat():
    print("📩 Audio received from Bean...")
    
    # 1. Receive Raw Audio & Fix it (Add WAV Header)
    raw_data = request.data
    wav_data = add_wav_header(raw_data)
    
    # Save to file for Gemini
    with open("input.wav", "wb") as f:
        f.write(wav_data)

    # 2. Send to Gemini
    print("🧠 Gemini Thinking...")
    try:
        # Upload audio to Gemini
        myfile = genai.upload_file("input.wav")
        
        # Prompt the AI
        response = model.generate_content([
            myfile, 
            "You are Bean, a helpful, calming robot companion. The user just spoke to you. Reply in one short, empathetic sentence (under 15 words)."
        ])
        reply_text = response.text
    except Exception as e:
        print(f"Gemini Error: {e}")
        reply_text = "I'm here with you, but I had trouble hearing that."
    
    print(f"🤖 Bean says: {reply_text}")

    # 3. Generate Audio Response (Edge TTS)
    output_file = "reply.mp3"
    asyncio.run(generate_audio(reply_text, output_file))

    # 4. Stream Audio Back to ESP32
    def generate():
        with open(output_file, "rb") as f:
            data = f.read(1024)
            while data:
                yield data
                data = f.read(1024)
    
    return Response(generate(), mimetype="audio/mpeg")

async def generate_audio(text, filename):
    # Uses a high-quality female voice. 
    # Options: "en-US-AnaNeural" (Female), "en-US-EricNeural" (Male)
    communicate = edge_tts.Communicate(text, "en-US-AnaNeural")
    await communicate.save(filename)

if __name__ == '__main__':
    # 0.0.0.0 makes it visible to the Hotspot Network
    app.run(host='0.0.0.0', port=5000)