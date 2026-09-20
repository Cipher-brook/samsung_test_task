SAMSUNG TEST TASK.

It was done in 2024.

This is the source release of the Samsung test task, version 1.0.0.
Supported Target CPU Architecture x86_64. Tested on linux kernel versions 6.8.0 x86_64.

How to build
To build:
    ~/package_directory$ make

To run:
    ~/package_directory$ sudo test_app/test_app

Directory Structure Layout
    ksrc/     kernel module files;
    test_app/ test application files;
    common/   shared common file between kernel module and test application;

Kernel module

A few notes about the stt_cdev devices to make it easier to understand.
The stt_cdev device supports writing and reading strings; other data is undefined.
The stt_cdev device doesn't support non-blocking input output.

The module creates and registers two character devices (Device_0, Device_1)
with the same major number but different minor numbers (0 and 1). The major
number for the character device is dynamically allocated.
The functionality for Device_0 and Device_1 is the same.

Each device supports the following operations:

* open:
    opens the appropriate device (Device_0 or Device_1);
* read:
    reads last written string from a database;
* write:
    writes the string to a database;
* ioctl:
    clears string database(delete all strings);
    sets the reference string;
* fasync:
    sends string match event notification to the application if newly
    written string is equal with the reference string (previously set by
    IOCTL command);
* release:
    closes device;

The module creates a folder in debug file system that corresponds to the
module name(stt_cdev) and file that corresponds to the minor number of the
specific device (0 or 1). When the file is read, it displays all the strings
that were written to the database of the specific device.

When the module was loaded the devices and files below were added to your
operating system:

1. /dev/Device_0 - supports open, read, write, ioctl, fasync, release operations
2. /dev/Device_1 - supports open, read, write, ioctl, fasync, release operations
3. /sys/kernel/debug/stt_cdev/0 - only supports read all written strings operation
4. /sys/kernel/debug/stt_cdev/1 - only supports read all written strings operation

The /package_directory/ksr/stt_cdev.c file provides additional kernel module documentation.
It may be generated to man or docs file by kernel-doc.

Test application.

The test application runs the following steps to check kernel module:

1. Loads kernel module to kernel, if module is existed than test application sends information about and continues work.
2. Initializes internal test application data, opens Device_0 and Device_1
3. Runs 4 tests for each device to test functionality:
    * Sets reference string by ioctl call for each of the devices;
    * Writes random strings to the Device_0 and Device_1;
    * Demonstrates that by reading debug files from debug file system it’s possible to
      read all the strings that were written to the particular device;
    * Demonstrates that string match event were sent by file asynchronous notification;
4. Demonstrates output test messages to console.
5. Unloads kernel module if it was not existed while loading.

Build files.

How to build and run kernel module and test application describes in the HOWTO.txt.
When you built files, path to built files will be:

    kernel module:    /package_directory/ksrc/stt_cdev.ko
    test application: /package_directory/test_app/test_app

All commands below should be executed from root directory of package.

HOW TO:

Build kernel module and test application:
    make

Clean kernel module and test application build files:
    make clean

Build kernel module:
    make ksrc

Clean kernel module build files:
    make clean_ksrc

Build test application:
    make test_app

Clean test application build files:
    make clean_test_app

Install kernel module:
    sudo make install_kmod

Uninstall kernel module:
    sudo make uninstall_kmod

Run test application:
    sudo test_app/test_app

Build kernel module and test application, run test application:
    make clean && make && sudo test_app/test_app
