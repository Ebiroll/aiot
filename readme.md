# Content

This repository contains information of speech recognition and aiot

Most notably it contains information regarding the sipeed

https://www.seeedstudio.com/sipeed

https://github.com/espressif/esp-adf/issues/73

# esp32-msc

This directory contains speech recognition on the esp32-msc board


# google_translate

This directory contains google translate from English to Swedish


# k210

SiPeed K210 example code.


# Building Pocket spinx

    git clone https://github.com/cmusphinx/sphinxbase.git
    git clone https://github.com/cmusphinx/pocketsphinx.git
    cd sphinxbase
    ./autogen.sh
    make
    cd ..
    cd pocketsphinx
    ./autogen.sh
    make


# Run withiut grammar

src/programs/pocketsphinx_continuous -inmic yes -hmm model/en-us/en-us -lm model/en-us/en-us.lm.bin  -dict model/en-us/cmudict-en-us.dict



# Adding Grammar (simple.jsgf)

    #JSGF V1.0;
    grammar all;
    public <all> = turn ( on | off ) the lights;


# Running with grammar


 src/programs/pocketsphinx_continuous -inmic yes -hmm model/en-us/en-us  -dict model/en-us/cmudict-en-us.dict  -jsgf simple.jsfg 


