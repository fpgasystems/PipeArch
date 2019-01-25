#include <iostream>
#include "iFPGA.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

int main(int argc, char* argv[]) {

	uint32_t numLines = 0;
	if (!(argc == 2)) {
		cout << "Usage: ./app <numLines>" << endl;
		return 0;
	}
	numLines = atoi(argv[1]);

	iFPGA myfpga(AFU_ACCEL_UUID);

	myfpga.ExampleApp(numLines);
}