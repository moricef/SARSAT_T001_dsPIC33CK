# Drivers

Drivers extraits de rf_interface.c

## Files

- mcp4922_driver.c/.h - MCP4922 dual DAC
- lmv358_buffer.c/.h - LMV358 buffers

## MCP4922
- 12-bit dual DAC
- SPI interface
- Functions: init, write_dac_a/b, write_both, shutdown

## LMV358
- Rail-to-rail buffers
- 3.3V → 1.0V scaling
- Functions: init, enable, set_iq_channels

Main code uses consolidated version in rf_interface.c