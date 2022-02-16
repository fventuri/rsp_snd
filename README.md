# rsp_snd - stream SDRplay RSP I/Q data to sound card (ALSA)

rsp_snd is a command line utility to stream I/Q data from a SDRplay RSP to a sound card using the ALSA sound system.

It is very similar both in functionality and command line arguments to the sdrplayalsa utility (see here: http://www.sdrutah.org/info/getting_iq_data_from_receiver.html and here: https://github.com/CrumResearch/sdrplayalsa).


## How to build and install

```
git clone https://github.com/fventuri/rsp_snd.git
cd rsp_snd
mkdir build
cd build
cmake ..
make
sudo make install
```


## Command line arguments

  - -a attack_ms   (AGC RSP model)
  - -a inc   (AGC GTW model) AGC "increase" threshold, default 16384
  - -B bwType baseband low-pass filter type (200, 300, 600, 1536, 5000)
  - -b dec   (AGC GTW model) AGC "decrease" threshold, default 8192
  - -C configfile
  - -c min   (AGC GTW model) AGC sample period (ms), default 500
  - -e gainfile  write gain values value to shared memory file
  - -f freq  set tuner frequency (in Hz)
  - -g agc_mode    (AGC RSP model)
  - -g gain  (AGC GTW model) set min gain reduction during AGC operation or fixed gain w/AGC disabled, default 30
  - -G gain  (AGC GTW model) set max gain reduction during AGC operation, default 59
  - -h       show usage
  - -i ser   specify input device (serial number)
  - -l val   set LNA state, default 3.  See SDRPlay API gain reduction tables for more info
  - -n agcmodel  AGC enable; AGC models: RSP, GTW - GTW uses parameters a,b,c,g,s,S,x,y,z
  - -o dev   specify output device
  - -r rate  set sampling rate (in Hz) [48000, 96000, 192000, 384000, 768000 recommended]
  - -S step_inc  (AGC GTW model) set gain AGC attenuation increase (gain reduction) step size in dB, default = 1 (1-10)
  - -s setPoint_dBfs   (AGC RSP model)
  - -s step_dec  (AGC GTW model) set gain AGC attenuation decrease (gain gain increase) step size in dB, default = 1 (1-10)
  - -v       enable verbose output
  - -W       enable wideband signal mode (e.g. half-band filtering). Warning: High CPU useage!
  - -x decay_ms   (AGC RSP model)
  - -x A     (AGC GTW model) num conversions for overload, default 4096
  - -y decay_delay_ms   (AGC RSP model)
  - -y B     (AGC GTW model) gain decrease event time (ms), default 100
  - -z decay_threshold_dB   (AGC RSP model)
  - -z C     (AGC GTW model) gain increase event time (ms), default 5000


## Configuration file example

```
# a comment
sample_rate = 768e3
agc_model = GTW

[rsp]
frequency = 14150e3
antenna = Antenna C
bw_type = 600
lna_state = 3
gain_file = /gainfile
```


## How to run rsp_snd


- using command line arguments:
```
rsp_snd -i 4798 -r 768000 -f 14150000 -n -B 600 -g 37 -l 3 -a 16384 -b 4096 -c 100 -x 1000 -y 200 -z 5000 -s 1 -S 3 -e /gainfile
```

- using a configuration file (command line arguments override settings in the configuration file)
```
rsp_snd -C rsp_snd.conf -f 7150000
```


## Credits

Many thanks to Gary Wong, AB1IP and Clint Turner, Ka7OEI for creating sdrplayalsa and their significant work on the AGC (model GTW)


## Copyright

(C) 2022 Franco Venturi - Licensed under the GNU GPL V3 (see [LICENSE](LICENSE))
