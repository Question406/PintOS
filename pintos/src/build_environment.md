# This file shows environment building of PintOS on Ubuntu


## Step1: Download PintOS
    https://cs.jhu.edu/~huang/cs318/fall17/pintos.tar.gz


## Step2: Prerequisites Install
    Bochs:
        1:  apt-get install libx11-dev libxrandr-dev
        2:  cd pintos/src/misc
            ./bochs-2.6.2-build.sh
    
    QEMU:
        1: apt-get install qemu-system-i386

## Step3: Build Utilities
    cd pintos/src/utilities
    make

    add pintos/src/utilities to $PATH

## Step4: Build VM image
    cd pintos/threads
    make

    we get a folder build

## Step5: Verify
    For Bochs check:
        cd pintos/threads/build
        pintos --bochs -- run alarm-zero
    should be like this

![avatar](/img/bochs.png)

    For QEMU check:
        cd pintos/threads/build
        make qemu -- run alarm-zero

    should be like this
![avatar](/img/qemu.png)