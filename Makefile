LIBS = -lportaudio -lm -lkissfft-float -lLCUI
RELEASE_FLAGS = -o3 -march=native -flto -funroll-loops -ffast-math -ftree-vectorize -fomit-frame-pointer -fvisibility=hidden -fopenmp -g0

pitch_tester: pitch_tester.c
	gcc $(RELEASE_FLAGS) $< -o $@ $(LIBS)

install: pitch_tester
	cp -r pitch_tester /usr/bin/

remove:
	rm -i /usr/bin/pitch_tester