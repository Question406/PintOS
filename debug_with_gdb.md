# This file shows an example of gdb debug of PintOS project

You must finish environment building, including utilities and add utilities into $PATH, otherwise Linux doesn't know where the executables are.

## Step1: Open virtual machine and let it wait for gdb connection
I use alarm-zero as an example

    cd /src/threads/build   (which we get it when we build environment)
    make                    (building the kernel image for PintOS)
    pintos --gdb --bochs -- run alarm-zero (running the virtual machine with Bochs)

should be like this, you can see this virtual machine isn't running, since it's waiting for gdb connecting to it and give commands
![avatar](/img/gdb_open_vm.png)


## Step2: Open gdb and connect to the VM we opened

    cd /src/threads/build
    pintos-gdb kernel.o         (pintos-gdb is one of the utilities we build)
should be like this
![avatar](/img/open_pintos-gdb.png)

## Step3: Connect pintos-gdb to VM and debug
these are commands in the gdb window

    debugpintos
    (use it as normal gdb, but with some different commands such as backtrace, which is a little different)
    see pintos.pdf Appendix E Debugging tools(page 108) for detail

should be like this 
![avatar](/img/debugging_withgdb.png)

below are some examples
![avatar](/img/debug_example.png)
![avatar](/img/debugging_example_2.png)