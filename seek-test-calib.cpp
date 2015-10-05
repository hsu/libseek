#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

#include "seek.hpp"

using namespace std;
using namespace LibSeek;

inline void sleep(float secs) {
	chrono::milliseconds dura(int(1000*secs));
	this_thread::sleep_for(dura);
}

int main() {
	setbuf(stdout, NULL);
	Imager iface;
	iface.init();

	Frame frame;

	iface.frame_init(frame);

	for (int i = 0; i < 100; i++) {
		iface.frame_acquire_raw(frame);
		int h = frame.height();
		int w = frame.width();
		char filename[30];
		sprintf(filename, "frame-%03d-raw.pgm", i);
		FILE * f = fopen(filename, "wb");
		int res = fprintf(f, "P5 %d %d 65535\n", w, h);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				uint16_t v = *&frame.rawdata()[2*(y*w+x)];
				v = le16toh(v);
				v = htobe16(v);
				res = fwrite((uint8_t*)&v, sizeof(uint16_t), 1, f);
			}
		}
		fclose(f);
	}

	iface.frame_exit(frame);
	iface.exit();
}
