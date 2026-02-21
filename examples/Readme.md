GSC 1.1 Support
---------------

GSC 1.1 support is a little more complex as the files are divided up and compressed with a text header. The instructions below show how to import GSC 1.1 into adb.

1. Download GSC1.1

  wget -r -t 5  ftp://cdsarc.u-strasbg.fr/pub/cats/I/254/

2. Download and build the decoder by running make in the src directory. This will generate some binaries with .exe file extensions.

3. Decode the GSC files

	./decode.exe GSC/N*/* > gsc.dat
	./decode.exe GSC/S*/* >> gsc.dat

4. There will now be a large decoded text file (about 1.5G) that can be imported with adb after stripping headers. 

4.1 blank line remove 

sed -i '/^$/d' gsc.dat

5. Please copy gsc.dat to your adb catalog directory I/220/gsc.dat and run 

 ./examples/gsc -i <path to your top level catalog dir>

There it will find the GSC data under I/254/gsc.dat and import it. Please note that this may take some time (about 2hrs on my laptop).

