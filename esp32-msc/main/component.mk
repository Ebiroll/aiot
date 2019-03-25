#
# Main Makefile. This is basically the same as a component makefile.
#
COMPONENT_ADD_LDFLAGS+= -L /home/olof/work/asr/aiot/esp32-msc/main/lib/ -lesp_wakenet -lnn_model_alexa_wn3
