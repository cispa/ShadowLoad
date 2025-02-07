# Short readme

- Compile the Collide+Power victim (`../../pf`) using `make`

- Check if the kernel module (`../module`) compiles

- Make sure the kernel module is loaded needed for measurements.

- Start the program runner:

```
sudo ./main output.npy
```

- run this once with   collide\_power\_loop\_rsb(); in the last line of main in `../../pf/collide_power.c`, once with collide\_power\_loop\_ref(), and once with collide\_power\_loop().

- the NUMBER_BYTES determines how fast the amplification is. currently set to a whole cache line



- Analyze:

Use and install:
https://github.com/0xhilbert/rda

```
rda - i output.npy - power_init --wcount=1 --bits=4 - per REnergy,IEnergy 1 99 - power_fit DEnergy - print
rda - i output.npy - power_init --wcount=1 --bits=4 - per REnergyPP0,IEnergyPP0 1 99 - power_fit DEnergyPP0 - print

rda - i output.npy - power_init --no_diff --wcount=1 --bits=4 - per Energy 1 99 - power_fit Energy - print
rda - i output.npy - power_init --no_diff --wcount=1 --bits=4 - per EnergyPP0 1 99 - power_fit EnergyPP0 - print

rda - i output.npy - power_init --wcount=1 --bits=4 - per RPower,IPower 1 99 - power_fit DPower - print
rda - i output.npy - power_init --wcount=1 --bits=4 - per RPowerPP0,IPowerPP0 1 99 - power_fit DPowerPP0 - print

rda - i output.npy - power_init --no_diff --wcount=1 --bits=4 - per Power 1 99 - power_fit Power - print
rda - i output.npy - power_init --no_diff --wcount=1 --bits=4 - per PowerPP0 1 99 - power_fit PowerPP0 - print

```
