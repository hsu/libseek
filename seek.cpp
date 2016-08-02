#include <cassert>
#include <cstring>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <tuple>

#include <endian.h>
#include <libusb.h>

#include "seek.hpp"
#include "seek_pimpl_impl.hpp"

using namespace std;
using namespace LibSeek;


#if !defined(DEBUG)
# define printf(...)
#else
# if defined(__ANDROID__)
#  include <android/log.h>
#  define LOG_TAG    "ZOUGLOUB-SEEK"
#  define printf(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
# else
#  define printf(...) fprintf(stderr, __VA_ARGS__)
# endif
#endif

/*!

  https://github.com/lod/seek-thermal-documentation/wiki/Seek-Software

*/
struct Request {
	enum Enum {
	 BEGIN_MEMORY_WRITE              = 82,
	 COMPLETE_MEMORY_WRITE           = 81,
	 GET_BIT_DATA                    = 59,
	 GET_CURRENT_COMMAND_ARRAY       = 68,
	 GET_DATA_PAGE                   = 65,
	 GET_DEFAULT_COMMAND_ARRAY       = 71,
	 GET_ERROR_CODE                  = 53,
	 GET_FACTORY_SETTINGS            = 88,
	 GET_FIRMWARE_INFO               = 78,
	 GET_IMAGE_PROCESSING_MODE       = 63,
	 GET_OPERATION_MODE              = 61,
	 GET_RDAC_ARRAY                  = 77,
	 GET_SHUTTER_POLARITY            = 57,
	 GET_VDAC_ARRAY                  = 74,
	 READ_CHIP_ID                    = 54,
	 RESET_DEVICE                    = 89,
	 SET_BIT_DATA_OFFSET             = 58,
	 SET_CURRENT_COMMAND_ARRAY       = 67,
	 SET_CURRENT_COMMAND_ARRAY_SIZE  = 66,
	 SET_DATA_PAGE                   = 64,
	 SET_DEFAULT_COMMAND_ARRAY       = 70,
	 SET_DEFAULT_COMMAND_ARRAY_SIZE  = 69,
	 SET_FACTORY_SETTINGS            = 87,
	 SET_FACTORY_SETTINGS_FEATURES   = 86,
	 SET_FIRMWARE_INFO_FEATURES      = 85,
	 SET_IMAGE_PROCESSING_MODE       = 62,
	 SET_OPERATION_MODE              = 60,
	 SET_RDAC_ARRAY                  = 76,
	 SET_RDAC_ARRAY_OFFSET_AND_ITEMS = 75,
	 SET_SHUTTER_POLARITY            = 56,
	 SET_VDAC_ARRAY                  = 73,
	 SET_VDAC_ARRAY_OFFSET_AND_ITEMS = 72,
	 START_GET_IMAGE_TRANSFER        = 83,
	 TARGET_PLATFORM                 = 84,
	 TOGGLE_SHUTTER                  = 55,
	 UPLOAD_FIRMWARE_ROW_SIZE        = 79,
	 WRITE_MEMORY_DATA               = 80,
	};
};


class Frame::impl {
 public:
	int width = 208; // 155; // 208;
	int height = 156; // 192; // 156;
	vector<uint8_t> rawdata;
	vector<uint16_t> data;
	void process();
};

Frame::Frame()
{
}

Frame::~Frame()
{
}

void Frame::impl::process()
{
}

class Imager::impl {
 public:
	struct libusb_context * ctx = NULL;
	struct libusb_device_handle * handle = NULL;
	struct libusb_device * dev = NULL;
	struct libusb_device_descriptor desc;

	Frame calib;

	vector<float> bpc_weights;
	vector<vector<tuple<char,char,int>>> bpc_kinds;
	vector<tuple<int,int,int>> bpc_list;

 public:
	impl();
	~impl();
	void init();
	void exit();

 public:
	void vendor_transfer(bool direction, uint8_t req, uint16_t value,
	 uint16_t index, vector<uint8_t> & data, int timeout=1000);

	void frame_get_one(Frame & frame);
};

inline void sleep(float secs)
{
	chrono::milliseconds dura(int(1000*secs));
	this_thread::sleep_for(dura);
}

int Frame::width()
{
	return m->width;
}

int Frame::height()
{
	return m->height;
}

uint16_t const * Frame::data()
{
	return &m->data[0];
}

uint8_t const * Frame::rawdata()
{
	return &m->rawdata[0];
}

Imager::impl::impl()
 : bpc_weights()
 , bpc_kinds()
 , bpc_list()
{
	printf("%s:%d\n", __PRETTY_FUNCTION__, __LINE__);

	ifstream is("/home/pi/seek_bpc_2.dat");
	// printf("/home/pi/libseek/seek_bpc_2.dat\n");

  // FILE *out = fopen("/tmp/out.xxx", "w");

	if (is.is_open()) {

		int nb_w;
		is >> nb_w;
		bpc_weights.resize(nb_w);
		for (int i = 0; i < nb_w; i++) {
			float v;
			is >> v;
			bpc_weights[i] = v;
      // fprintf(out, "w[%d] %f\n", i, bpc_weights[i]); fflush(out);
		}

    // fprintf(out, "nb_w %d\n",nb_w); fflush(out);

		int nb_k;
		is >> nb_k;
    // fprintf(out, "nb_k %d\n", nb_k); fflush(out);
		bpc_kinds.resize(nb_k);
		for (int i = 0; i < nb_k; i++) {
      // fprintf(out, "i %d\n", i); fflush(out);
			int nb_c;
			is >> nb_c;
      // fprintf(out, "nb_c %d\n", nb_c); fflush(out);
      // fprintf(out, "bpc_kinds %d\n", bpc_kinds.size()); fflush(out);
      // fprintf(out, "bpc_kinds[%d] before %d\n", i, bpc_kinds[i].size()); fflush(out);
			bpc_kinds[i].resize(nb_c);
      // fprintf(out, "bpc_kinds[%d] after %d\n", i, bpc_kinds[i].size()); fflush(out);
			for (int j = 0; j < nb_c; j++) {
        // fprintf(out, "j %d\n", j); fflush(out);
				int dx, dy, iw;
				is >> dy >> dx >> iw;
				bpc_kinds[i][j] = make_tuple(dx, dy, iw);
        // fprintf(out, "bpc_kinds[%d][%d]=(%d, %d, %d)\n", i, j, dx, dy, iw); fflush(out);
			}
      // fprintf(out, "nb_k %d nb_c %d\n",nb_k, nb_c); fflush(out);
		}

    // fprintf(out, "nb_k %d\n", nb_k); fflush(out);
		int nb_bp;
		is >> nb_bp;
    // fprintf(out, "nb_bp %d\n",nb_bp); fflush(out);
		bpc_list.resize(nb_bp);
		for (int i = 0; i < nb_bp; i++) {
			int x, y, ik;
			is >> y >> x >> ik;
			bpc_list[i] = make_tuple(x, y, ik);
		}
    // fprintf(out, "nb_bp %d\n",nb_bp); fflush(out);
    // fclose(out);

	}

}

Imager::impl::~impl()
{
	exit();
}

void Imager::impl::init()
{
	int res;

	if (handle != NULL) {
		throw runtime_error("dev should be null");
	}

	// Init libusb
	res = libusb_init(&ctx);
	if (res < 0) {
		throw runtime_error("Failed to initialize libusb");
	}

	struct libusb_device **devs;
	ssize_t cnt;

	// Get a list os USB devices
	cnt = libusb_get_device_list(ctx, &devs);
	if (cnt < 0) {
		throw runtime_error("No devices");
	}

	bool found(false);
	for (int idx_dev = 0; idx_dev < cnt; idx_dev++) {
		dev = devs[idx_dev];
		res = libusb_get_device_descriptor(dev, &desc);
		if (res < 0) {
			libusb_free_device_list(devs, 1);
			throw runtime_error("Failed to get device descriptor");
		}

		res = libusb_open(dev, &handle);
		if (res == 0 && desc.idVendor == 0x289d && desc.idProduct == 0x0010) {
			found = true;
			printf("Found!\n");
			break;
		}

	} // for each device

	if (!found) {
		libusb_free_device_list(devs, 1);
		throw runtime_error("Seek not found");
	}

	printf("\nDevice found");

	int config2;
	res = libusb_get_configuration(handle, &config2);
	if (res != 0) {
		libusb_free_device_list(devs, 1);
		throw runtime_error("Couldn't get device configuration");
	}
	printf("\nConfigured value : %d",config2);

	if (config2 != 1) {
		res = libusb_set_configuration(handle, 1);
		if (res != 0) {
			libusb_free_device_list(devs, 1);
			throw runtime_error("Couldn't set device configuration");
		}
	}

	libusb_free_device_list(devs, 1);

	res = libusb_claim_interface(handle, 0);
	if (res < 0) {
		throw runtime_error("Couldn't claim interface");
	}
	printf("\nClaimed Interface\n");

	// device setup sequence

	try {
		vector<uint8_t> data = {0x01};
		char req(Request::TARGET_PLATFORM);
		vendor_transfer(0, req, 0, 0, data);
	}
	catch (...) {
		// Try deinit device and repeat.
		vector<uint8_t> data = { 0x00, 0x00 };
		{
			char req(Request::SET_OPERATION_MODE);
			vendor_transfer(0, req, 0, 0, data);
			vendor_transfer(0, req, 0, 0, data);
			vendor_transfer(0, req, 0, 0, data);
		}

		{
			char req(Request::TARGET_PLATFORM);
			vendor_transfer(0, req, 0, 0, data);
		}
	}

	{
		char req(Request::SET_OPERATION_MODE);
		vector<uint8_t> data = {0x00, 0x00};
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::GET_FIRMWARE_INFO);
		vector<uint8_t> data(4);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::READ_CHIP_ID);
		vector<uint8_t> data(12);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}


	{
		char req(Request::SET_FACTORY_SETTINGS_FEATURES);
		vector<uint8_t> data = { 0x20, 0x00, 0x30, 0x00, 0x00, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::GET_FACTORY_SETTINGS);
		vector<uint8_t> data(64);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::SET_FACTORY_SETTINGS_FEATURES);
		vector<uint8_t> data = { 0x20, 0x00, 0x50, 0x00, 0x00, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}


	{
		char req(Request::GET_FACTORY_SETTINGS);
		vector<uint8_t> data(64);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::SET_FACTORY_SETTINGS_FEATURES);
		vector<uint8_t> data = { 0x0c, 0x00, 0x70, 0x00, 0x00, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}


	{
		char req(Request::GET_FACTORY_SETTINGS);
		vector<uint8_t> data(24);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::SET_FACTORY_SETTINGS_FEATURES);
		vector<uint8_t> data = { 0x06, 0x00, 0x08, 0x00, 0x00, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::GET_FACTORY_SETTINGS);
		vector<uint8_t> data(12);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::SET_IMAGE_PROCESSING_MODE);
		vector<uint8_t> data = { 0x08, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::GET_OPERATION_MODE);
		vector<uint8_t> data(2);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}

	{
		char req(Request::SET_IMAGE_PROCESSING_MODE);
		vector<uint8_t> data = { 0x08, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::SET_OPERATION_MODE);
		vector<uint8_t> data = { 0x01, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
	}

	{
		char req(Request::GET_OPERATION_MODE);
		vector<uint8_t> data(2);
		vendor_transfer(1, req, 0, 0, data);
		printf("Response: ");
		for (int i = 0; i < data.size(); i++) {
			printf(" %2x", data[i]);
		}
		printf("\n");
	}
}


void Imager::impl::exit()
{
	int res;

	if (handle == NULL) {
		return;
	}

	{
		char req(Request::SET_OPERATION_MODE);
		vector<uint8_t> data = { 0x00, 0x00 };
		vendor_transfer(0, req, 0, 0, data);
		vendor_transfer(0, req, 0, 0, data);
		vendor_transfer(0, req, 0, 0, data);
	}

	if (handle != NULL) {
		res = libusb_release_interface(handle, 0);
		libusb_close(handle);
		handle = NULL;
	}

	if (ctx != NULL) {
		libusb_exit(ctx);
		ctx = NULL;
	}
}

void Imager::impl::vendor_transfer(bool direction,
 uint8_t req, uint16_t value, uint16_t index, vector<uint8_t> & data,
 int timeout)
{
	int res;
	uint8_t bmRequestType = (direction ? LIBUSB_ENDPOINT_IN : LIBUSB_ENDPOINT_OUT)
	 | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE;
	uint8_t bRequest = req;
	uint16_t wValue = value;
	uint16_t wIndex = index;

	if (data.size() == 0) {
		data.reserve(16);
	}

	uint8_t * aData = data.data();
	uint16_t wLength = data.size();
	if (!direction) {
		// to device
		printf("ctrl_transfer(0x%x, 0x%x, 0x%x, 0x%x, %d)",
		 bmRequestType, bRequest, wValue, wIndex, wLength);
		printf(" [");
		for (int i = 0; i < wLength; i++) {
			printf(" %02x", data[i]);
		}
		printf(" ]");

		res = libusb_control_transfer(handle, bmRequestType, bRequest,
		 wValue, wIndex, aData, wLength, timeout);

		if (res != wLength) {
			printf("\x1B[31;1mBad returned length: %d\x1B[0m\n", res);
		}
		printf("\n");
	}
	else {
		// from device
		printf("ctrl_transfer(0x%x, 0x%x, 0x%x, 0x%x, %d)",
		 bmRequestType, bRequest, wValue, wIndex, wLength);
		res = libusb_control_transfer(handle, bmRequestType, bRequest,
		 wValue, wIndex, aData, wLength, timeout);
		if (res != wLength) {
			printf("Bad returned length: %d\n", res);
		}
		printf(" -> [");
		for (auto & x: data) {
			printf(" %02x", x);
		}
		printf("]\n");
	}
}


Imager::Imager()
{
}

Imager::~Imager()
{
	m->exit();
}

void Imager::init()
{
	m->init();
}

void Imager::exit()
{
	m->exit();
}

void Imager::frame_init(Frame & frame)
{
	m->calib.m->rawdata.resize(frame.width()*frame.height()*2);
	m->calib.m->data.resize(frame.width()*frame.height());
	frame.m->rawdata.resize(frame.width()*frame.height()*2);
	frame.m->data.resize(frame.width()*frame.height());
}

void Imager::frame_exit(Frame & frame)
{
}


void Imager::impl::frame_get_one(Frame & frame)
{
	int res;

	int size = frame.width() * frame.height();

	{ // request a frame
		vector<uint8_t> data = { uint8_t(size & 0xff), uint8_t((size>>8)&0xff), 0, 0 };
		vendor_transfer(0, 0x53, 0, 0, data);
	}

	int bufsize = size * sizeof(uint16_t);

	{
		int todo = bufsize;
		int done = 0;

    /*
    fprintf(stderr, "LIBUSB_ERROR_TIMEOUT (-7) %d\n", LIBUSB_ERROR_TIMEOUT);
    fprintf(stderr, "LIBUSB_ERROR_PIPE (-9) %d\n", LIBUSB_ERROR_PIPE);
    fprintf(stderr, "LIBUSB_ERROR_OVERFLOW (-8) %d\n", LIBUSB_ERROR_OVERFLOW);
    fprintf(stderr, "LIBUSB_ERROR_NO_DEVICE (-4) %d\n", LIBUSB_ERROR_NO_DEVICE);
    */
		while (todo != 0) {
			int actual_length = 0;
			printf("Asking for %d B of data at %d\n", todo, done);
			res = libusb_bulk_transfer(handle, 0x81, &frame.m->rawdata[done],
			 todo, &actual_length, 500);
			if (res != 0) {
				fprintf(stderr, "\x1B[31;1m%s: libusb_bulk_transfer returned %d\x1B[0m\n", __func__, res);
        Imager::impl::exit();
        ::sleep(1);
        Imager::impl::init();
        ::sleep(1);
			  fprintf(stderr, "Actual length %d todo %d \n", actual_length, todo);
			  todo = 0;
			}
			printf("Actual length %d todo: %d\n", actual_length, todo);
			todo -= actual_length;
			done += actual_length;
		}
	}
}

void Imager::frame_acquire_raw(Frame & frame)
{
	m->frame_get_one(frame);
	vector<uint8_t> & rawdata = frame.m->rawdata;

	uint8_t status = rawdata[20];
	printf("Status byte: %2x\n", status);
}

void Imager::frame_acquire(Frame & frame)
{
	while (true) {
		frame_acquire_raw(frame);
		vector<uint8_t> & rawdata = frame.m->rawdata;

		uint8_t status = rawdata[20];

		if (status == 1) {
			m->calib.m->rawdata = frame.m->rawdata;
			printf("Calib\n");
			continue;
		}

		if (status != 3) {
			printf("Bad\n");
			continue;
		}

    // FILE *out = fopen("/tmp/apply.xxx", "a+");

		vector<uint16_t> & data = frame.m->data;

		int h = frame.height();
		int w = frame.width();

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				int a;

				uint16_t v = reinterpret_cast<uint16_t*>
				 (rawdata.data())[y*w+x];
				v = le16toh(v);

				uint16_t v_cal = reinterpret_cast<uint16_t*>
				 (m->calib.m->rawdata.data())[y*w+x];
				v_cal = le16toh(v_cal);

				a = int(v) - int(v_cal);

				// level shift
				a += 0x8000;

				if (a < 0) {
					a = 0;
				}
				if (a > 0xFFFF) {
					a = 0xFFFF;
				}

				v = a;

				data[y*w+x] = v;
			}
		}


		for (int idx_bp = 0; idx_bp < m->bpc_list.size(); idx_bp++) {
			int x, y, ik;
			tie(x, y, ik) = m->bpc_list[idx_bp];
		  // fprintf(out, "x, y, ik[%d, %d, %d]\n", x, y, ik);
			auto cnt = m->bpc_kinds[ik];
			float v = 0;
			float avg = 0;
		  // fprintf(out, "data[%d] before %d\n", y*w+x, data[y*w+x]); fflush(out);
			for (int idx_pt = 0; idx_pt < cnt.size(); idx_pt++) {
				int8_t dx, dy;
				int iw;
				tie(dx, dy, iw) = cnt[idx_pt];
			  // fprintf(out, "    idx_pt %d dx %d dy %d iw %d\n", idx_pt, dx, dy, iw); fflush(out);
				avg += data[(y+dy)*w+(x+dx)] / cnt.size();
				float corr = data[(y+dy)*w+(x+dx)] * m->bpc_weights[iw];
				v = v+corr;
        // fprintf(out, "    data[%d][%d] w[%f] corr[%f] v[%f]\n", (y+dy)*w+(x+dx), data[(y+dy)*w+(x+dx)], m->bpc_weights[iw], corr, v); fflush(out);
			}
			data[y*w+x] = v;
			// data[y*w+x] += v;
			// data[y*w+x] = avg;
		  // fprintf(out, "data[%d] after %d v[%f]\n", y*w+x, data[y*w+x], v); fflush(out);
		}
    // fclose(out);

		break;
	}
}

