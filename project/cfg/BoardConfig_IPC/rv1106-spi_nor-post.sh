#!/bin/bash

cd $RK_PROJECT_PACKAGE_OEM_DIR/usr/lib/
rm librknnmrt_rockauto.so
rm librockauto.so
rm librkdemuxer.so
rm libjpeg.so*
rm auto_lane_obj_det.data
rm librockit_full.so
rm librockit_tiny.so
cd -

rm $RK_PROJECT_PACKAGE_ROOTFS_DIR/lib/libstdc++.so.6.0.25-gdb.py

# delete nouse ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/gcm.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/ccm.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/sha256_generic.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/libaes.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/libsha256.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/gf128mul.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/cmac.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/libarc4.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/aes_generic.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/ctr.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/mac80211.ko
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/ko/atmb_iot_supplicant_demo
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/cgi-fcgi
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/dumpsys
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/j2s4b_dev
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/modetest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/mpi_enc_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/mpp_info_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rgaImDemo
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_adc_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rkaiq_3A_server
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rkaiq_tool_server
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_event_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_gpio_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rkipc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rkisp_demo
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_led_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_adec_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_aenc_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_af_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_ai_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_amix_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_ao_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_avio_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_avs_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_dup_venc_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_mb_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_mmz_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_rgn_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_sys_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_tde_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_vi_dup_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_vi_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_mpi_vpss_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_pwm_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_rve_sample_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_system_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_time_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/rk_watchdog_test
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_ai
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_ai_aenc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_ai_aenc_adec_ao_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_avs
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_avs_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_aiisp
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_aiisp_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_dual_aiisp
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_dual_aiisp_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_dual_camera
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_dual_camera_wrap
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_multi_camera_eptz
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_vi_avs_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_vi_avs_venc_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_vi_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_demo_vi_venc_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_isp_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_mulit_isp_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_multi_vi
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_multi_vi_avs
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_multi_vi_avs_osd_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_rgn_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_rv1103_dual_memory_opt
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_venc_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_vi
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_vi_vpss_osd_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/sample_vpss_stresstest
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_adec_bind_ao
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_adec_bind_ao_external_decoder
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_ai_bind_aenc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_ai_bind_aenc_external_encoder
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_ai_get_frame
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_ao_send_frame
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_cmd.txt
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_ivs
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_change_resolution_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_chn_sharebuf_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_combo_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_jpeg
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_osd
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_rtsp
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_rtsp_dev_chn_sharebuf_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_rtsp_eptz
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_rtsp_three_camera_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_svc_rtsp
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_venc_wrap_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_bind_vpss_bind_venc
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_get_frame
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_get_frame_rkaiq
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_get_frame_send_vo_rv1106
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/simple_vi_get_frame_tde
rm -f $RK_PROJECT_PACKAGE_OEM_DIR/usr/bin/vpu_api_test
