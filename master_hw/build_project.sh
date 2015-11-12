#!/bin/bash

if [[ ! -e zc706_dma_10g ]]; then
    vivado -mode batch -source project.tcl
fi

if [[ -e zc706_dma_10g && ! -d zc706_dma_10g ]]; then
    echo "zc706_dma_10g exists but is not a directory. Aborting."
    exit -1
fi

vivado -mode batch -source impl.tcl
