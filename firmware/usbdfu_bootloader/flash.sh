#!/bin/bash

openocd_path="/opt/openocd/0.10.0-201610281609-dev/bin/"
hexFile="$1"
elfFile="$2"

echo -e "\nDebug information:\nopenocd_path: $openocd_path\nhexFile: $hexFile\n\n"

"$openocd_path/openocd" -f "$openocd_path/../scripts/interface/ftdi/jtag-lock-pick_tiny_2.cfg" -c "transport select swd;" -f "$openocd_path/../scripts/target/stm32f1x.cfg" -c "adapter_khz 400; reset_config trst_open_drain; init; reset init; flash write_image erase  $hexFile ; verify_image $hexFile ; reset run; shutdown;"


#"$openocd_path/openocd" -f "$openocd_path/../scripts/interface/ftdi/jtag-lock-pick_tiny_2.cfg" -c "transport select swd;" -f "$openocd_path/../scripts/target/stm32f1x.cfg" -c "adapter_khz 400; reset_config trst_open_drain; init; program $elfFile verify reset exit"
