# Phantom Link: Encrypted Wireless Covert Channel [Active - WIP]

Have you ever wanted to use Wi-Fi in the middle of the desert?
Phantom Link is a standalone, encrypted, bidirectional wireless communication device built from scratch using two Raspberry Pi Pico 2 W microcontrollers. 

This project demonstrates low-level hardware hacking, bypassing standard Wi-Fi networking stacks to establish a custom, encrypted UDP link. Messages are encrypted, transmitted over raw 2.4GHz RF, decrypted, and displayed on standalone OLED hardware.

## Architecture

The system operates without a standard Wi-Fi router. The Receiver (RX) acts as a private Access Point, and the Transmitter (TX) connects as a client via a forced static IP to bypass DHCP timing constraints. 

Once the link is established, the TX packages plaintext into a custom C `struct`, encrypts the payload, and blasts it over a custom UDP socket. The RX intercepts the packet, verifies the size and sync word, decrypts the payload, and updates the SPI OLED screen.

## Key Features

*   **Custom Encryption:** Utilizes an XOR cipher (moving to Speck in future updates) to encrypt payloads before transmission.
*   **Custom Packet Structs:** Packets are packed with `__attribute__((packed))` to ensure byte-for-byte alignment across the link.
*   **Pixel-Perfect UI Pipeline:** Developed a custom Python/ImageMagick pipeline to convert 128x64 PNG assets into 1024-byte C-arrays, bypassing bloated graphics libraries and achieving a true 1:1 pixel ratio on the SH1106 OLED.
*   **Hardware Interfacing:** Custom SPI driver written from scratch for the OLED, and analog joystick input via the ADC.

## Tech Stack

*   **Language:** C (C11)
*   **SDK:** Raspberry Pi Pico SDK
*   **Networking:** lwIP (Lightweight IP) stack, UDP
*   **Build System:** CMake (compiled entirely in WSL Ubuntu)
*   **Hardware:** 2x Pico 2 W, 2x Inland 1.3" SH1106 SPI OLEDs, 2x Sony PSP 1000 analog joysticks, breadboards, buttons.

## Gallery

### [Proof-of-Concept] First Success Sending and Receiving an Encrypted Payload
<img width="3072" height="3072" alt="IMG_20261101_175008064_HDR" src="https://github.com/user-attachments/assets/de9636a9-d38e-4655-bdd7-4e771551e8ba" />

### [Final Beta] UI/UX With Sony PSP-1000 Joystick Fully Functional
<img width="3072" height="3072" alt="IMG_20261110_190128444_HDR" src="https://github.com/user-attachments/assets/423bf832-ab03-407f-b7ee-3536018320ac" />



### 2.5 second startup screen mockup 128x64 [beta] using Lopaka
<img width="1919" height="1079" alt="Screenshot 2026-11-03 105327" src="https://github.com/user-attachments/assets/71efdb70-433e-48aa-a295-f887d4b67758" />

### 1:1 Pixel Representation achieved using a 1024-byte buffer [Final Beta]
<img width="3072" height="3072" alt="IMG_20261103_192202006_HDR" src="https://github.com/user-attachments/assets/6170b1bc-dd5d-44b3-99db-df62017fa892" />

### Main-screen design layout using Pixilart 128x64 [Alpha]
<img width="1919" height="1079" alt="Screenshot 2026-11-04 192518" src="https://github.com/user-attachments/assets/aa58658a-4a1d-4623-a15f-11a7f7b2fac7" />


### Detailed Serial Port Monitoring [Fixed Brownout Issue]
<img width="1919" height="1079" alt="Screenshot 2026-11-01 143836" src="https://github.com/user-attachments/assets/3093cd82-b947-4402-a433-2caf3119a111" />


## Roadmap

- [x] Establish encrypted UDP link over private AP.
- [x] Develop custom SPI OLED driver and PNG-to-C pipeline.
- [x] Implement T9 text-input UI (Alpha/Numeric states) with joystick navigation.
- [ ] Create manual sprites for each typable character and symbol for custom font insertion in o(n) runtime.
- [ ] Implement button functionality and menu wrap-around logic
- [ ] Upgrade payload encryption to Speck Cipher.
- [ ] Implement MAC-layer steganography (StegoFrameOrder) for stealth signaling.
- [ ] [Undecided/Optional] Dig up antenna trace to solder intel 2.4 ghz antenna on each Pico for signal amplification.
