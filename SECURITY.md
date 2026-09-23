# Security

## Never commit secrets

Do not commit:

- Gemini API keys
- Wi-Fi passwords
- Private network details
- Any other credentials

Use placeholders in public source code:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";
```

## If a key is accidentally pushed

Assume it is compromised.

1. Revoke the key in Google AI Studio.
2. Create a new API key.
3. Update your local firmware.
4. Remove the secret from the repository.
5. Remember that deleting a file does not necessarily remove the secret from Git history.

## Production devices

Embedding a permanent cloud API key directly in firmware is convenient for a personal prototype but is not appropriate for a product distributed to users.

For production, use a backend service or short-lived token mechanism so the permanent API credential is not stored on the device.

