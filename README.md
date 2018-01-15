Installation
------------

SOFTWARE:
- Plug BeagleBone into computer via miniUSB cable. It has a g_ether gadget that you should be able to ping at 192.168.7.2. It _should_ DHCP-assign your computer an IP of 192.168.7.1, but usually fails to do so, in which case just set it statically.
- ssh debian@192.168.7.2. Password is `temppwd`.
- You will need internet. Plug ethernet into network and edit `/etc/network/interfaces` to enable:
    auto eth0
    iface eth0 inet dhcp
- From home directory, git clone https://github.com/shield-ai/rtlizer.git
- sudo apt-get install socat build-essential cmake libusb-1.0-0-dev
- git clone git://git.osmocom.org/rtl-sdr.git
- mkdir -p rtl-sdr/build
- cd rtl-sdr/build
- cmake -DDETACH_KERNEL_DRIVER=on -DINSTALL_UDEV_RULES=on ..
- make
- sudo make install
- sudo ldconfig
- cd ~/rtlizer/server
- make
- ln -s rtlizer/start.sh start.sh
- sudo nano /etc/rc.local
  - before the `exit 0`, add `/home/debian/start.sh &`
- Now set ethernet to static to talk to skywalker. Edit `/etc/network/interfaces`:
    auto eth0
    iface eth0 inet static
        address 192.168.0.10
        netmask 255.255.255.0

HARDWARE:
- Physically install BeagleBone on Skywalker quadrotor
- Provide 5V to barrel jack, or miniUSB port if necessary
- Connect the ethernet cable that would go to a LIDAR
- Plug NanoSDR into USB port, MCX-SMA adapter into NanoSDR, and antenna into SMA adapter

Usage
-----

On the beaglebone, see `~/rtlizer/start.sh`. If you set `fake_data` to true, it will
send random numbers, and if `false`, it will send a spectrum analysis.

On skywalker, run `shockwave_proxy.sh`.

On the ground station, run `fake_ground_station` or `real_ground_station`, espective of the value of `fake_data` on the beaglebone.
