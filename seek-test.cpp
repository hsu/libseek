#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <math.h>

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

  int _max = 0;
  int _min = 0xffff;

	for (int i = 0; i < 71370537; i++) {
		//break;
		iface.frame_acquire(frame);

		int h = frame.height();
		int w = frame.width();
    // fprintf(stderr, "h %d w %d\n", h, w);
		vector<uint16_t> img(3*w*h);
		{
			int _max_cur = 0;
			int _min_cur = 0xffff;
			int _margin = 0;
#if 1
			for (int y = _margin; y < h-_margin; y++) {
				for (int x = _margin; x < w-_margin; x++) {
					uint16_t v = frame.data()[y*w+x];
					if (v > _max_cur) _max_cur = v;
					if (v < _min_cur) _min_cur = v;
				}
			}

      // make more dramatic
      _max_cur = _max_cur - 0x0100;
      if (_max_cur < _min_cur) _max_cur = _min_cur + 0x0020;
      if (_max_cur > 0xFFFF) _max_cur = 0xFFFF;
      _min_cur = _min_cur - 0x0000;
      if (_min_cur < 0x0000) _min_cur = 0x0000;
#elif 0
			_max_cur = 0x8200;
			_min_cur = 0x7e00;
#else
			_max_cur = 0xffff;
			_min_cur = 0x0000;
#endif

      float eps = 0.1;
      _max = (1.0 - eps) * _max + eps * _max_cur;
      _min = (1.0 - eps) * _min + eps * _min_cur;

			for (int y = 0; y < h; y++) {
				for (int x = 0; x < w; x++) {
          // for (int c = 0; c < 3; c++) {
#if 1
            float v = float(frame.data()[y*w+x] - _min) / (_max - _min);
            if (v < 0.0) { v = 0; }
            if (v > 1.0) { v = 1; }
            uint16_t o = 0xffff * v;
#else
            uint16_t o = frame.data()[y*w+x];
#endif
            // https://www.particleincell.com/2014/colormap/
            uint16_t r = 0;
            uint16_t g = 0;
            uint16_t b = 0;

            double step = (1.0 - v) / 0.25;  // [0 to 4)
            int step_int = floor(step);
            uint16_t scalar = floor(0xffff*(v - step));
            if (step_int == 0)
            {
              r = 0xFFFF;
              g = scalar;
              b = 0;
            }
            else if (step_int == 1)
            {
              r = 0xFFFF-scalar;
              g = 0xFFFF;
              b = 0;
            }
            else if (step_int == 2)
            {
              r = 0;
              g = 0xFFFF;
              b = scalar;
            }
            else if (step_int == 3)
            {
              r = 0;
              g = 0xFFFF-scalar;
              b = 0xFFFF;
            }
            else if (step_int >= 4)
            {
              r = 0;
              g = 0;
              b = 0xFFFF;
            }

            //fprintf(stderr, " %4x", o);
            img[3*(y*w+x)+0] = r;
            img[3*(y*w+x)+1] = g;
            img[3*(y*w+x)+2] = b;
          // }
				}
				//fprintf(stderr, "\n");
			}
			//fprintf(stderr, "\n");
			fwrite((uint8_t*)img.data(), sizeof(uint16_t), 3*w*h, stdout);
		}

	}

	iface.frame_exit(frame);

	iface.exit();
}

