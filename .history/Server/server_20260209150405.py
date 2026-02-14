import os
import asyncio
import struct
from flask import Flask, request, send_file
import google.generativeai as genai
import edge_tts

# --- CONFIG ---
# PASTE YOUR KEY HERE
os.environ["GOOGLE_API_KEY"] = "AIzaSyDE-mAJ7NpPICQqSeIoGV-rvfO4k99L2Qc"
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

app = Flask(__name__)

# --- FIX 1: USE A MODEL FROM YOUR LIST ---
# We are switching to 2.0 Flash because your list confirmed you have it.
model = genai.GenerativeModel("gemini-flash-latest") 

REPLY_FILE = "reply.mp3"

def add_wav_header(pcm_data, sample_rate=16000, channels=1, bit_depth=16):
    header = struct.pack('<4sI4s4sIHHIIHH4sI',
        b'RIFF', 36 + len(pcm_data), b'WAVE', b'fmt ', 16, 1, channels,
        sample_rate, sample_rate * channels * (bit_depth // 8),
        channels * (bit_depth // 8), bit_depth, b'data', len(pcm_data)
    )
    return header + pcm_data

@app.route('/chat', methods=['POST'])
def chat():
    print("📩 Audio received...")
    # Add header so Gemini knows it's 16kHz audio
    wav_data = add_wav_header(request.data, sample_rate=16000)
    
    with open("input.wav", "wb") as f:
        f.write(wav_data)

    print("🧠 Gemini Thinking...")
    try:
        myfile = genai.upload_file("input.wav")
        response = model.generate_content([
            myfile, 
            "You are Bean, a witty and helpful AI robot. Listen to the audio and answer the user's question directly. Keep your response under 20 words."
        ])
        
        # --- FIX 2: THE "STENING" CUT-OFF FIX ---
        # "Ok, " gives the speaker time to wake up.
        # So instead of "Stening...", you hear "Ok, I am listening."
        text_to_speak = "Ok, " + response.text
        print(f"✅ Bean says: {text_to_speak}")

    except Exception as e:
        print(f"❌ GEMINI ERROR: {e}")
        # Fallback with shield word
        text_to_speak = "Ok, I am listening."

    # Generate MP3
    asyncio.run(generate_audio(text_to_speak, REPLY_FILE))
    return "READY"

@app.route('/reply.mp3', methods=['GET'])
def get_audio():
    print("📤 Sending MP3 to Bean...")
    return send_file(REPLY_FILE, mimetype="audio/mpeg")

async def generate_audio(text, filename):
    communicate = edge_tts.Communicate(text, "en-US-AnaNeural")
    await communicate.save(filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)