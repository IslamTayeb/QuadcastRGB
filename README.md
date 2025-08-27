# QuadcastRGB - USB Hub Compatible Fork

This is a modified version of QuadcastRGB that fixes USB hub compatibility issues for HyperX DuoCast/Quadcast S microphones.

## The Problem

The original version fails with "HyperX Quadcast S isn't connected" when the microphone is connected through a USB hub (like an Anker hub). This happens because:

1. **USB Enumeration Timing**: USB hubs introduce delays in device enumeration that the original code doesn't account for
2. **Interface Claiming**: The original code attempts to claim USB interfaces immediately without retry logic, which fails with hubs
3. **Device Detection**: No retry mechanism for device discovery through intermediate USB devices

## The Solution

This fork adds:

- Retry logic with delays for USB device enumeration (3 attempts with 500ms delays)
- Retry logic for claiming USB interfaces (3 attempts with 100ms delays)
- Retry logic for opening USB devices (3 attempts with 200ms delays)
- Better error messages that mention USB accessibility issues
- Debug logging to troubleshoot connection issues

## Setup Instructions (macOS M3 with USB Hub)

### Prerequisites

1. Install libusb via Homebrew:

   ```bash
   brew install libusb
   ```

2. Ensure your DuoCast is connected to your Anker USB hub

### Building from Source

1. Clone this repository:

   ```bash
   git clone https://github.com/yourusername/QuadcastRGB.git
   cd QuadcastRGB
   ```

2. Build with macOS-specific settings:

   ```bash
   make clean
   make OS=macos
   ```

   For debug version (shows device detection process):

   ```bash
   make clean
   make dev OS=macos
   ```

### Running

The program requires sudo on macOS due to USB device permissions:

```bash
# Set solid red color (default)
sudo ./quadcastrgb solid

# Set custom color
sudo ./quadcastrgb solid 00ff00  # green

# Other modes
sudo ./quadcastrgb blink
sudo ./quadcastrgb cycle
sudo ./quadcastrgb wave
sudo ./quadcastrgb pulse

# Stop the RGB control
sudo killall quadcastrgb
```

### Troubleshooting

1. **Device not found**: Run the debug version to see device enumeration:

   ```bash
   sudo ./dev solid
   ```

2. **Permission denied**: Always use `sudo` on macOS

3. **Check if your device is detected**:

   ```bash
   lsusb | grep -E "(DuoCast|Quadcast)"
   # or on macOS:
   system_profiler SPUSBDataType | grep -A 10 "DuoCast"
   ```

   You should see:
   - `03f0:098c` - HyperX DuoCast Controller (RGB control)
   - `03f0:0a8c` - HyperX DuoCast (audio device)

## Technical Details

### What Changed

1. **modules/devio.c**:
   - Added retry loops in `open_micro()` for device enumeration
   - Added retry logic in `claim_dev_interface()` for interface claiming
   - Added delays between device checks in `dev_search()`
   - Improved error messages to mention USB accessibility
   - Added debug logging throughout device detection

2. **Makefile**:
   - Added macOS-specific libusb include paths: `-I/opt/homebrew/opt/libusb/include`
   - Added macOS-specific library paths: `-L/opt/homebrew/opt/libusb/lib`

### USB Hub Compatibility

The modifications specifically address:

- **Timing Issues**: USB hubs may not immediately present devices to libusb
- **Power Management**: Some hubs may delay device initialization
- **Interface Access**: Hubs can affect how quickly USB interfaces become available

The retry mechanisms give the USB subsystem time to properly enumerate and initialize devices through the hub.

## Auto-Start on Login (10% Gray)

To have the RGB lights automatically set to 10% gray when your Mac starts:

### LaunchDaemon Setup

This method runs the program automatically on system startup and keeps it running.

1. **Install the program system-wide**:

   ```bash
   sudo cp ./quadcastrgb /usr/local/bin/
   sudo chmod 755 /usr/local/bin/quadcastrgb
   ```

2. **Install the LaunchDaemon**:

   ```bash
   sudo cp com.islamtayeb.quadcastrgb.plist /Library/LaunchDaemons/
   sudo chmod 644 /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
   sudo chown root:wheel /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
   ```

3. **Load the daemon** (starts immediately and on every boot):

   ```bash
   sudo launchctl load -w /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
   ```

4. **Check if it's running**:

   ```bash
   sudo launchctl list | grep quadcastrgb
   ps aux | grep quadcastrgb
   ```

5. **View logs if needed**:

   ```bash
   tail -f /tmp/quadcastrgb.log
   tail -f /tmp/quadcastrgb.error.log
   ```

### Managing the Auto-Start

```bash
# Stop the service
sudo launchctl unload /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist

# Start the service
sudo launchctl load /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist

# Disable auto-start (but keep installed)
sudo launchctl unload -w /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist

# Re-enable auto-start
sudo launchctl load -w /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist

# Completely remove
sudo launchctl unload -w /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
sudo rm /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
sudo rm /usr/local/bin/quadcastrgb
```

### Changing the Color

#### Easy Method: Color Changer Utility

Use the included `quadcastrgb-color` utility for quick color changes:

```bash
# Install the utility
sudo cp quadcastrgb-color /usr/local/bin/
sudo chmod +x /usr/local/bin/quadcastrgb-color

# Change colors with simple commands:
sudo quadcastrgb-color FF0000      # Red
sudo quadcastrgb-color 00FF00      # Green
sudo quadcastrgb-color 0000FF      # Blue
sudo quadcastrgb-color FFFFFF      # White
sudo quadcastrgb-color 000000      # Black (off)
sudo quadcastrgb-color 1A1A1A      # 10% gray

# Or use preset names:
sudo quadcastrgb-color red
sudo quadcastrgb-color blue
sudo quadcastrgb-color white
sudo quadcastrgb-color off         # Turn off (black)

# Show current color:
sudo quadcastrgb-color
```

Available presets: black, white, red, green, blue, yellow, cyan, magenta, orange, purple, pink, gray, darkgray, lightgray, dimgray, off

#### Manual Method

To manually change the color, edit the plist file:

```bash
sudo nano /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
```

Find the line with the color code and change it to your desired color, then reload:

```bash
sudo launchctl unload /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
sudo launchctl load /Library/LaunchDaemons/com.islamtayeb.quadcastrgb.plist
```

### How It Works

The QuadcastRGB program runs continuously in the background:

- The program has an internal loop that reapplies the color settings every ~20ms
- It automatically handles USB disconnections and reconnections
- It runs indefinitely until terminated with a signal (SIGINT or SIGTERM)
- The LaunchDaemon is configured with `KeepAlive: SuccessfulExit: false` to restart only if it crashes unexpectedly

**USB Disconnection Handling:**

- When you unplug the mic, the program detects the disconnection
- It continues running and attempts to reconnect every 2 seconds
- When you replug the mic, it automatically reconnects and restores your color settings
- No rainbow effect when replugging - your chosen color is immediately applied!

### About the Rainbow Effect

You may notice a brief rainbow effect when:

- Adjusting the microphone volume
- Muting/unmuting the microphone
- Turning the microphone on/off
- Unplugging and reconnecting the USB

This is normal behavior. The DuoCast briefly returns to its default rainbow state when the hardware state changes, but the continuous restart loop quickly reapplies your chosen solid color.

## Original Project

Based on [QuadcastRGB](https://gitlab.com/Ors1mer/QuadcastRGB) by Ors1mer.
