#!/bin/bash

cd hls
vivado_hls build.tcl
cd ../

if [[ ! -e zc706_10g_streamingg ]]; then
    vivado -mode batch -source project.tcl
fi

if [[ -e zc706_10g_streaming && ! -d zc706_10g_streaming ]]; then
    echo "zc706_10g_streaming exists but is not a directory. Aborting."
    exit -1
fi

vivado -mode batch -source impl.tcl
