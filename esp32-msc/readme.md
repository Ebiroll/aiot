# Build
Note that main/component.mk contains hardcoded paths to the libraries
This because the esp32_sr module was removed from head of git

The program will light another led, each time you say "alexa"


# Set environment before building
export ADF_PATH=/home/olof/esp/esp-adf
make menuconfig

# Make sure to
copy the directory  en-us to the sdcard 
Set main task size to 9000
Enable extra ram size.  ( ESP32-WROVER) 