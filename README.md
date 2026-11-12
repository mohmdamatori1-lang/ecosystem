# Ecosystem App

A cross-platform Qt file sharing app for local networks. Devices auto-discover each other via UDP multicast and transfer files directly over TCP — no server required.

---

## Features

- **Auto discovery** — UDP multicast (`224.0.0.167:53317`) announces devices every 2s
- **Direct P2P transfer** — Files streamed over TCP port `8080`
- **Cross-platform** — Linux, macOS, Windows, Android
- **Simple GUI** — "Devices" tab to scan, "Transfer" tab to send

---

## Requirements

- Qt 5/6 (Widgets)
- C++17 (uses `std::filesystem`, `std::thread`)
- **Windows:** `Ws2_32.lib` (auto-linked) · **Linux/macOS:** POSIX sockets · **Android:** multicast permissions + `WifiManager.MulticastLock` (handled in `main()`)

---

## Build

```bash
qmake ecosystem.pro
make
./ecosystem-1.0
```

Or open `ecosystem.pro` in **Qt Creator** and press Run.

**Android:** add these to `AndroidManifest.xml`:
```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

---

## Usage

1. Launch the app on two devices on the **same network**.
2. **Devices tab → Scan** to find a peer's IP.
3. **Transfer tab → Browse → Send** to send a file.
4. Received files are saved to the **current working directory**.

---

## How It Works

- **Discovery:** Each app multicasts `"SERVER"` every 2s. `findServer()` joins the group and returns the first responder's IP (3s timeout).
- **Transfer:** Sender connects to `<peer-ip>:8080` and sends a header (`filename_len`, `filename`, `file_size`), followed by the file in 8KB chunks.

| Setting | Value |
|---|---|
| Multicast group | `224.0.0.167` |
| Discovery port | `53317` |
| Transfer port | `8080` |
| Chunk size | `8192 B` |

---

## Security

For **trusted local networks only**. No encryption, no authentication, and received files are not filename-sanitized (path traversal risk). Do not expose to untrusted networks.

---

## Limitations

- Only the **first** discovered peer is shown
- No progress bar, cancel, or resume
- No encryption or auth

---

## License

Specify your license (e.g., MIT).
