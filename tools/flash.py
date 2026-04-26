"""
STM32 UART Bootloader Flash Tool

Usage:
    python flash.py COM_PORT path/to/app.bin

Example:
    python flash.py COM3 ../app/out/app.bin

Requirements:
    pip install pyserial
"""

import sys
import struct
import time
import serial

# These must match the bootloader protocol defines
CMD_SYNC = 0xA5
CMD_DATA = 0xB6
CMD_DONE = 0xC7
RSP_ACK  = 0x06
RSP_NACK = 0x15

PACKET_SIZE = 128
BAUD_RATE = 115200


def crc32(data):
    """Same CRC32 algorithm as the one on the MCU side."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF


def wait_ack(ser):
    """Read one byte from serial, return True if ACK, False if NACK."""
    rsp = ser.read(1)
    if len(rsp) == 0:
        print("ERROR: timeout waiting for response")
        sys.exit(1)
    if rsp[0] == RSP_ACK:
        return True
    elif rsp[0] == RSP_NACK:
        return False
    else:
        print(f"ERROR: unexpected response 0x{rsp[0]:02X}")
        sys.exit(1)


def main():
    if len(sys.argv) != 3:
        print("Usage: python flash.py COM_PORT firmware.bin")
        sys.exit(1)

    port = sys.argv[1]
    bin_path = sys.argv[2]

    # Step 1: read the .bin file
    with open(bin_path, "rb") as f:
        firmware = f.read()

    fw_size = len(firmware)
    print(f"Firmware: {bin_path} ({fw_size} bytes)")

    # Step 2: open serial port
    ser = serial.Serial(port, BAUD_RATE, timeout=10)
    time.sleep(0.1)
    ser.reset_input_buffer()

    # Step 3: sync with bootloader
    print("Sending sync...")
    ser.write(bytes([CMD_SYNC]))
    if not wait_ack(ser):
        print("ERROR: sync failed")
        sys.exit(1)
    print("Synced with bootloader")

    # Step 4: send firmware size (4 bytes, little endian)
    ser.write(struct.pack("<I", fw_size))
    if not wait_ack(ser):
        print("ERROR: size rejected")
        sys.exit(1)
    print(f"Size accepted: {fw_size} bytes")

    # Step 5: wait for MCU to finish erasing flash (takes a few seconds)
    print("Waiting for flash erase...")
    ser.timeout = 30
    if not wait_ack(ser):
        print("ERROR: erase failed")
        sys.exit(1)
    ser.timeout = 10
    print("Flash erased")

    # Step 6: send firmware in 128 byte packets, each with its own CRC
    offset = 0
    total_packets = (fw_size + PACKET_SIZE - 1) // PACKET_SIZE
    packet_num = 0

    while offset < fw_size:
        chunk = firmware[offset:offset + PACKET_SIZE]
        chunk_crc = crc32(chunk)

        # Send: command byte + data + crc
        ser.write(bytes([CMD_DATA]))
        ser.write(chunk)
        ser.write(struct.pack("<I", chunk_crc))

        if not wait_ack(ser):
            print(f"\nPacket {packet_num} NACK, retrying...")
            continue

        offset += len(chunk)
        packet_num += 1
        pct = int(offset * 100 / fw_size)
        print(f"\r  [{packet_num}/{total_packets}] {pct}%", end="", flush=True)

    print("\nAll packets sent")

    # Step 7: send DONE command + CRC of entire firmware for final verification
    total_crc = crc32(firmware)
    ser.write(bytes([CMD_DONE]))
    ser.write(struct.pack("<I", total_crc))

    if wait_ack(ser):
        print("Verification OK, update successful!")
    else:
        print("ERROR: final CRC verification failed")
        sys.exit(1)

    ser.close()


if __name__ == "__main__":
    main()
