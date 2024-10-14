# Rockchip sample Makefile

.PHONY : clean


all:$(COMM_OBJ)
	@mkdir -p ./lib
	@$(RK_APP_CROSS)-ar crs ./lib/libsample_comm.a $(COMM_OBJ)

$(COMM_OBJ): $(COMM_SRC)
	@$(RK_APP_CROSS)-gcc -c $(patsubst %.o,%.c,$@)  -o $@ $(SAMPLE_CFLAGS) $(LD_FLAGS)

clean:
	@make -C isp2.x clean
	@make -C isp3.x clean
	@rm  -f *.o
	@rm -rf $(COMM_DIR)/lib

