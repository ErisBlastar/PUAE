# P-UAE
#
# 2006-2011 Mustafa TUFAN (aka GnoStiC/BRONX)
#
#
#
base=" --with-sdl --with-sdl-gl --with-cocoa-gfx --with-sdl-sound --enable-drvsnd "
cd32=" --enable-cd32 "
a600=" --enable-gayle "
scsi=" --enable-scsi-device --enable-ncr --enable-a2091 "
other=" --with-caps --enable-amax --enable-gccopt"
#
#
./bootstrap.sh
./configure $base $cd32 $a600 $scsi $other CFLAGS="-m32" LDFLAGS="-m32" CPPFLAGS="-m32"
make clean
make
