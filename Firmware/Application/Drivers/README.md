# Application drivers

Protocol drivers for external devices belong here. The first planned modules are
W25Q64 SPI NOR Flash and AT24C02 I2C EEPROM. CubeMX owns peripheral and pin
initialisation; each driver owns its device protocol behind a small interface.
