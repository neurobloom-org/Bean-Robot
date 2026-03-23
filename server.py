import os
import asyncio
import google.generativeai as genai
import edge_tts

# --- CONFIG ---
# PASTE YOUR NEW KEY HERE (Please revoke the old one!)
os.environ["GOOGLE_API_KEY"] = "AIzaSyBuAwFEo0ripI3-MO1QML_FGlnjryhnX7k"
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

# Initialize the model
model = genai.GenerativeModel("gemini-2.5-flash")

REPLY_FILE = "reply.mp3"

async def generate_audio(text, filename):
    """Generates the text-to-speech MP3 file."""
    communicate = edge_tts.Communicate(text, "en-US-AnaNeural")
    await communicate.save(filename)

async def main():
    print("🤖 Bean Terminal is READY!")
    print("Type your message below. Type 'exit' to quit.\n")
    
    while True:
        # 1. Get text input from the terminal
        user_input = input("🗣️ You: ")
        
        # Allow the user to exit the loop
        if user_input.lower() in ['exit', 'quit']:
            print("👋 Goodbye!")
            break
            
        if not user_input.strip():
            continue

        print("🧠 Bean is thinking...")
        
        try:
            # 2. Send the typed text to Gemini
            prompt = (
                "You are Bean, a witty and helpful AI robot. Answer the user's question directly. "
                f"Keep your response under 20 words.\n\nUser asks: {user_input}"
            )
            response = model.generate_content(prompt)
            
            # The 'Ok,' fix to give the speaker time to wake up
            text_to_speak = "Ok, " + response.text.replace('\n', ' ').strip()
            print(f"✅ Bean says: {text_to_speak}")
            
            # 3. Generate the MP3 file in the current folder
            print("🎙️ Generating MP3...")
            await generate_audio(text_to_speak, REPLY_FILE)
            print(f"📁 Audio saved to {REPLY_FILE} in your folder!\n")

        except Exception as e:
            print(f"❌ ERROR: {e}\n")

if __name__ == '__main__':
    # Run the async main loop
    asyncio.run(main())