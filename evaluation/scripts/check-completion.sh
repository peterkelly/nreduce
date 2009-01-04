#!/bin/bash

echo Total jobs: `for i in */*; do echo $i; done | wc -l`

for i in */*; do
  if ! grep -q 'with status 0' $i/output; then
    if ! grep -q 'with status' $i/output; then
      echo $i: did not complete
    else
      echo $i: `grep 'with status' $i/output`
    fi
  fi
done
