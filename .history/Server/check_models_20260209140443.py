import google.generativeai as genai
import os

# PASTE YOUR KEY HERE
os.environ["GOOGLE_API_KEY"] = "YOUR_GEMINI_API_KEY"
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

print("Searching for available models...")
for m in genai.list_models():
    if 'generateContent' in m.supported_generation_methods:
        print(m.name)