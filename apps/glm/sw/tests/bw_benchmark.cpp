#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"

#define VALUE_TO_INT_SCALER 10

#define FPGA

int main(int argc, char* argv[]) {

	uint32_t numLinesToRead = 32;
	uint32_t numLinesToWrite = 32;
	uint32_t numIterations = 1;
	uint32_t numInstances = 1;
	if (!(argc == 5)) {
		cout << "Usage: ./app <numLinesToRead> <numLinesToWrite> <numIterations> <numInstances>" << endl;
		return 0;
	}
	numLinesToRead = atoi(argv[1]);
	numLinesToWrite = atoi(argv[2]);
	numIterations = atoi(argv[3]);
	numInstances = atoi(argv[4]);

	xDevice xdevice;
	Server* server[iFPGA::MAX_NUM_BANKS];
	for (uint32_t i = 0; i < iFPGA::MAX_NUM_BANKS; i++) {
		server[i] = new Server(false, false, i, &xdevice);
	}
	vector<FPGA_ColumnML*> columnMLs;
	for (uint32_t i = 0; i < numInstances; i++) {
		FPGA_ColumnML* temp = new FPGA_ColumnML(server[i%iFPGA::MAX_NUM_BANKS]);
		temp->ReadBandwidth(numLinesToRead, numLinesToWrite, numIterations);
		columnMLs.push_back(temp);
	}

	vector<FThread*> fthreads;
	double start = get_time();
	for (uint32_t i = 0; i < numInstances; i++) {
		FThread* temp = server[i%iFPGA::MAX_NUM_BANKS]->Request(columnMLs[i]);
		fthreads.push_back(temp);
	}
	for (uint32_t i = 0; i < numInstances; i++) {
		fthreads[i]->WaitUntilFinished();
	}
	double total = get_time() - start;

	float readSize = ((float)64*numLinesToRead*numIterations/1e9)*numInstances;
	float writeSize = ((float)64*numLinesToWrite*numIterations/1e9)*numInstances;

	cout << "Read size in DRAM: " << readSize << " GB" << endl;
	cout << "Write size in DRAM: " << writeSize << " GB" << endl;
	cout << "Time: " << total << endl;
	cout << "Read rate: " << readSize/total << " GB/s" << endl;
	cout << "Write rate: " << writeSize/total << " GB/s" << endl;

	// Verify
	for (uint32_t i = 0; i < numInstances; i++) {
#ifdef XILINX
		columnMLs[i]->CopyInputHandleFromFPGA();
#endif

		bool error = false;
		auto output = iFPGA::CastToFloat(columnMLs[i]->m_inputHandle) + numIterations*numLinesToRead*16;
		for (uint32_t e = 0; e < numIterations; e++) {
			for (uint32_t i = 0; i < numLinesToWrite*16; i++) {
				// cout << output[e*numLinesToWrite*16 + i] << endl;
				if (output[e*numLinesToWrite*16 + i] != e*numLinesToRead*16 + i) {
					cout << "Mismatch at " << e*numLinesToWrite*16 + i << ": " << output[e*numLinesToWrite*16 + i] << endl;
					error = true;
					break;
				}
			}
		}
		if (error == false) {
			cout << "PASS!" << endl;
		}
		else {
			cout << "FAIL!" << endl;
		}
	}

	for (uint32_t i = 0; i < numInstances; i++) {
		delete columnMLs[i];
	}
	columnMLs.clear();
	for (uint32_t i = 0; i < iFPGA::MAX_NUM_BANKS; i++) {
		delete server[i];
	}

	return 0;
}