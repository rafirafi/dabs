CC=gcc
CPP=g++
CFLAGS=-Wall -O3 -fomit-frame-pointer -pthread -I. -march=native -ffast-math

#CFLAGS=-Wall -g -pthread -I.
#CFLAGS=-Wall -O3 -pthread -I. -march=native -mavx2 -pg

#you may remove those two lines I think, it's just for my system
CFLAGS += -I /home/sed/uhd/install/include
UHD_FOR_ME = -Wl,-rpath=/home/sed/uhd/install/lib -L/home/sed/uhd/install/lib

#comment those three lines to disable UHD support
CFLAGS += -DHAVE_UHD_SDR
UHDLIB = -luhd -lboost_system
UHDOBJ = sdr/usrp.o sdr/usrp_low.o

#comment those three lines to disable RTL-SDR support
CFLAGS += -DHAVE_RTL_SDR
RTLLIB = -lrtlsdr
RTLSDROBJ = sdr/rtlsdr.o sdr/rtlsdr_low.o

ALSALIB = -lasound

VERSION = 1.0

dabs: dabs.o utils/file.o phy/frequency_offset.o utils/fft.o phy/channel.o \
      phy/signal.o phy/qpsk.o transport/fic.o coding/puncture.o \
      coding/viterbi_decode.o coding/energy_dispersal.o \
      coding/frequency_interleaving.o utils/crc.o transport/fib.o \
      transport/msc.o coding/time_interleaving.o audio/super_frame.o \
      utils/bit_reader.o audio/aac.o sdr/sdr.o utils/io.o \
      audio/alsa.o utils/log.o utils/scan.o utils/database.o \
      utils/band3.o audio/eq.o \
      $(UHDOBJ) $(RTLSDROBJ)
	$(CPP) $(CFLAGS) -o $@ $^ -lfftw3f -lm -lfaad $(UHD_FOR_ME) $(UHDLIB) \
               $(RTLLIB) $(ALSALIB)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cc
	$(CPP) $(CFLAGS) -c -o $@ $<

clean:
	rm -f dabs *.o phy/*.o utils/*.o transport/*.o coding/*.o audio/*.o \
           sdr/*.o

release: clean
	rm -f dabs-$(VERSION).tar.bz2
	rm -rf dabs-$(VERSION)
	mkdir dabs-$(VERSION)
	cp README AUTHORS Makefile dabs.c dabs-$(VERSION)
	cp -r audio coding phy sdr transport utils dabs-$(VERSION)
	tar cvf dabs-$(VERSION).tar dabs-$(VERSION)
	rm -rf dabs-$(VERSION)
	bzip2 -9 dabs-$(VERSION).tar
