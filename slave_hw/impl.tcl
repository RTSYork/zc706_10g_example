open_project zc706_10g_streaming/zc706_10g_streaming.xpr

reset_run impl_1
reset_run synth_1

launch_runs synth_1
wait_on_run synth_1
launch_runs -to_step write_bitstream impl_1
wait_on_run impl_1
