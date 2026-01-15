#!/bin/bash

if [ ! -d ./bootstrap ]; then
  mkdir bootstrap
fi

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

make bootstrap
