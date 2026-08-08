# Bootloader components

Place reusable Bootloader services here. Each component must expose a small
hardware-independent interface; hardware access belongs in `bsp/`.

Planned first components:

- image metadata and validation;
- external W25Q128 storage access interface;
- upgrade transaction journal;
- SHA-256 and ECDSA-P256 verification adapters.
