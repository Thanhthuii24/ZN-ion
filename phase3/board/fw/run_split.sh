#!/bin/zsh
# Phase 3 board (v6 Zn-ion) for one split, after v5_auto/phase3/board/fw/run_split.sh.
#   run_split.sh build 2021|42|2024   host only: generate C-models (enc fp32, fade INT8, head fp32) + val data, build Appli
#   run_split.sh flash 2021|42|2024   BOARD: flash Appli to XSPI2 (external loader, no option bytes), reset, capture UART
# Boot (internal flash) is the template Boot that jumps to 0x70000000; flash it only if Appli does not start.
set -e
MODE=$1; SPLIT=$2; typeset -A EID; EID=(2021 ZN-E010 42 ZN-E013 2024 ZN-E014); E=${EID[$SPLIT]}
FW=${0:A:h}; B=$FW/..; X=$B/../export; R=$B/../..; PY=/opt/homebrew/Caskroom/miniforge/base/envs/battery/bin/python
ST=/Applications/ST/STEdgeAI/4.1/Utilities/macarm/stedgeai; BUN="$HOME/Library/Application Support/stm32cube/bundles"
PRG="$BUN/programmer/2.23.0/bin/STM32_Programmer_CLI"; CM="$BUN/cmake/4.3.1+st.1/CMake.app/Contents/bin/cmake"
export PATH="$BUN/gnu-tools-for-stm32/14.3.1+st.2/bin:$BUN/ninja/$(ls $BUN/ninja)/bin:$PATH"
if [[ $MODE == build ]]; then
  rm -rf $FW/net_enc $FW/net_fade $FW/net_head $FW/ws_enc $FW/ws_fade $FW/ws_head $FW/proj/build $FW/proj/Appli/build $FW/proj/Boot/build
  $ST generate -m $X/${E}_E.onnx      --name enc  --target stm32h7 --workspace $FW/ws_enc  --output $FW/net_enc  >/dev/null
  $ST generate -m $B/${E}_F_int8.onnx --name fade --target stm32h7 --input-data-type float32 --output-data-type float32 --workspace $FW/ws_fade --output $FW/net_fade >/dev/null
  $ST generate -m $X/${E}_H.onnx      --name head --target stm32h7 --workspace $FW/ws_head --output $FW/net_head >/dev/null
  (cd $R && BL_SPLIT=$SPLIT $PY $FW/gen_fw_data.py | tail -1)
  (cd $FW/proj && "$CM" --preset Release >/dev/null && "$CM" --build --preset Release 2>&1 | grep -E "error|warning: .*overflow|FLASH|RAM|FAILED|Linking" || true)
  ls -la $FW/proj/Appli/build/*Appli.elf $FW/proj/Boot/build/*Boot.elf 2>/dev/null || ls -R $FW/proj/build | grep -i elf
elif [[ $MODE == flash ]]; then
  ELF=$(ls $FW/proj/Appli/build/*Appli.elf 2>/dev/null || find $FW/proj/build -name "*Appli.elf" | head -1)
  (cd $FW/proj && "$PRG" -c port=SWD mode=UR -d $ELF --extload flasher/MX25UW25645G_NUCLEO-H7S3L8.stldr -q | grep -E "complete|Error")
  mkdir -p $B/onboard
  ($PY $FW/serial_capture.py /dev/cu.usbmodem11302 $B/onboard/stream_$SPLIT.log 600) &
  sleep 2; "$PRG" -c port=SWD mode=HOTPLUG -hardRst -q | grep -E "Hard reset|Error"; wait
fi
