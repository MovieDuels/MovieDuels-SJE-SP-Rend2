#!/bin/bash
GAMEDATA="build-linux/release/GameData"

mkdir -p $GAMEDATA

cmake -S . -B build-linux #\
  #-DCMAKE_BUILD_TYPE=RelWithDebInfo \
  #-DBuildSPEngine=ON -DBuildSPGame=ON -DBuildSPRdVanilla=ON -DBuildSPRend2=ON

cmake --build build-linux -j"$(nproc)"

install -m755 build-linux/MovieDuels-SP.x86_64 "$GAMEDATA/"
install -m755 build-linux/code/rd-vanilla/MovieDuels-rdsp_x86_64.so "$GAMEDATA/"
install -m755 build-linux/code/rd-rend2/MovieDuels-rdsp-rend2_x86_64.so "$GAMEDATA/"
install -m755 build-linux/code/game/MovieDuels-gamex86_64.so "$GAMEDATA/"
