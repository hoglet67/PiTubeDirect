all: tuberom_dominic65816.inc

clean:
	rm -f *.o *.lst *.bin *.inc *~

%.o : %.as
	ca65 -l $<.lst $<

%.bin: %.o
	ld65 $< -o $@ -C $*.lkr

%.inc: %.bin
	xxd -i -c 16 $< > $@
	cp $@ ../$*.c
