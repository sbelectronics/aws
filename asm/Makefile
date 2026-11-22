copyup: rats.run
	#python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker test.asm test.asm
	#python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker test2.asm test2.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker macros.inc macros.inc
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker print.inc print.inc
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker file.inc file.inc
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker forth.asm forth.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker params.asm params.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker hexdump.asm hexdump.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker sum.asm sum.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker crc.asm crc.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker maze.asm maze.asm
	python ../../ctostool/ctostool.py ../../converted/img/DSKA0264-incoming-aws2.img replace baker rats.run rats.run

rats.run: ../../rats/work/aws/Rats.run
	cp ../../rats/work/aws/Rats.run rats.run

imgup:
	scp ../../converted/img/DSKA0264-incoming-aws2.img smbaker@pi-gotek.lan:/mnt/usb_drive/files/

imgdown:
	scp smbaker@pi-gotek.lan:/mnt/usb_drive/files/DSKA0264-incoming-aws2.img ../../converted/img
