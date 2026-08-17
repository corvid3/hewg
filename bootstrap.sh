#!/bin/bash

if [ ! -d ./bootstrap ]; then
  mkdir bootstrap
fi

! echo "starting bootstrap of dependencies"
(
  cd bootstrap  

  # lexible must come first,
  # scl depends upon it
  if [ ! -d ./lexible ]; then
    git clone https://github.com/corvid3/lexible lexible
  fi

  (
    cd lexible
    git pull
    ./bootstrap.sh
  )

  if [ ! -d ./scl ]; then
    git clone https://github.com/corvid3/libscl scl
  fi

  (
    cd scl
    git pull
    ./bootstrap.sh
  )

  if [ ! -d ./datalogpp ]; then
    git clone https://github.com/corvid3/datalogpp datalogpp
  fi

  (
    cd datalogpp
    git pull
    ./bootstrap.sh
  )

  if [ ! -d ./jayson ]; then
    git clone https://github.com/corvid3/jayson jayson
  fi

  (
    cd jayson
    git pull
    ./bootstrap.sh
  )

  if [ ! -d ./terse ]; then
    git clone https://github.com/corvid3/terse terse
  fi

  (
    cd terse
    git pull
    ./bootstrap.sh
  )
)

! echo "bootstrap of dependencies finished, starting hewg bootstrap"
make bootstrap

! echo "setting up targetfiles"
mkdir ~/.hewg/targets
cp ./x86-linux-gnu ~/.hewg/targets
cp ./x86-linux-clang ~/.hewg/targets

! echo "now we actually run the hewg bootstrap"
./bin/hewg build --install

! echo "you may now use hewg. put ~/.hewg/bin into your $PATH or something."
