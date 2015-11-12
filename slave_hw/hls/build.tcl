open_project "zc706_streaming_example"
open_solution "solution1"

set_top top
add_files main.cpp

# Use VC707
set_part {xc7z045ffg900-2}

create_clock -period 6.4 -name default

# Create some hardware
csynth_design

# And dump to ISE
export_design -format ip_catalog -description "Example HLS packet processor" -vendor "york.ac.uk" -version "1.0"
