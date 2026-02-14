import google.generativeai as genai
import os

# PASTE YOUR KEY HERE
os.environ["GOOGLE_API_KEY"] = "AIzaSyD52V5oYThjh4vhxbts1jolQv6T-uEfYwA"
genai.configure(api_key=os.environ["GOOGLE_API_KEY"])

print("Searching for available models...")
for m in genai.list_models():
    if 'generateContent' in m.supported_generation_methods:
        print(m.name)