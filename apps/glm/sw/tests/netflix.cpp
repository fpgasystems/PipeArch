#include <iostream>
#include "FPGA_LRMF.h"
#include "Server.h"

using namespace std;

#define FPGA

int main(int argc, char* argv[]) {

	char* pathToDataset;
	int Mdim = -1;
	int Udim = -1;
	uint32_t numFeatures = 0;
	uint32_t tileSize = 0;
	bool asyncUpdate = false;
	uint32_t numEpochs = 10;
	uint32_t numInstances = 1;
	uint32_t sw0hw1 = 0;
	bool staleRead = false;
	if (!(argc == 11)) {
		cout << "Usage: ./app <pathToDataset> <Mdim> <Udim> <numFeatures> <tileSize> <asyncUpdate> <staleRead> <numEpochs> <numInstances> <sw0hw1>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	Mdim = atoi(argv[2]);
	Udim = atoi(argv[3]);
	numFeatures = atoi(argv[4]);
	tileSize = atoi(argv[5]);
	asyncUpdate = (strcmp(argv[6], "y") == 0);
	staleRead = (strcmp(argv[7], "y") == 0);
	numEpochs = atoi(argv[8]);
	numInstances = atoi(argv[9]);
	sw0hw1 = atoi(argv[10]);

	float stepSize = 0.01;
	float lambda = 0;

#ifdef FPGA
	xDevice xdevice;
	Server server(false, false, 0, &xdevice);
	vector<FPGA_LRMF*> lrmf;
	lrmf.reserve(numInstances*numInstances);

	for (uint32_t i = 0; i < numInstances*numInstances; i++) {
		if (i == 0)
			lrmf.push_back( new FPGA_LRMF(&server, numFeatures) );
		else
			lrmf.push_back( new FPGA_LRMF(&server) );
	}
#else
	vector<LRMF*> lrmf;
	lrmf.push_back( new LRMF(numFeatures));
#endif

	if ( strcmp(pathToDataset, "syn") == 0) {
		lrmf[0]->GenerateSyntheticData(Mdim, Udim, 0.05);
	}
	else {
		lrmf[0]->ReadNetflixData(pathToDataset, Mdim, Udim);
	}

	lrmf[0]->DivideLBIntoTiles(tileSize);

	if (sw0hw1 == 0) {
		// lrmf[0]->RandInitMU();
		// lrmf[0]->OptimizeLR(stepSize, lambda, numEpochs);

		// lrmf[0]->RandInitMU();
		// lrmf[0]->Optimize(stepSize, lambda, numEpochs);

		// lrmf[0]->RandInitMU();
		// lrmf[0]->OptimizeNaive(stepSize, lambda, numEpochs);

		lrmf[0]->RandInitMU();
		if (!staleRead) {
			lrmf[0]->OptimizeRound(stepSize, lambda, numEpochs);
		}
		else {
			lrmf[0]->OptimizeRoundStale(stepSize, lambda, numEpochs);
		}

		// lrmf[0]->RandInitMU();
		// lrmf[0]->OptimizeRoundMulti(stepSize, lambda, numEpochs, numInstances);
	}
	else {
#ifdef FPGA
		lrmf[0]->RandInitMU();

		lrmf[0]->CreateMemoryLayout();
		for (uint32_t i = 1; i < numInstances*numInstances; i++) {
			lrmf[i]->UseCreatedMemoryLayout(lrmf[0]);
		}

		vector<uint32_t> MtileToStart;
		vector<uint32_t> numTilesM;
		vector<uint32_t> UtileToStart;
		vector<uint32_t> numTilesU;
		lrmf[0]->DivideWorkByThread(numInstances, MtileToStart, numTilesM, UtileToStart, numTilesU);

		for (uint32_t i = 0; i < numInstances; i++) {
			for (uint32_t j = 0; j < numInstances; j++) {
				bool fit = lrmf[i*numInstances+j]->fOptimizeRound(
					MtileToStart[i*numInstances+j], numTilesM[i*numInstances+j],
					UtileToStart[i*numInstances+j], numTilesU[i*numInstances+j],
					stepSize, lambda, asyncUpdate, staleRead, 1);
				if (!fit)
					return 1;
			}
		}

		server.PreCopy(lrmf[0]);

		double total = 0.0;

		for (uint32_t e = 0; e < numEpochs; e++) {
			double start = get_time();

			for (uint32_t i = 0; i < numInstances; i++) {
				vector<FThread*> fthreads;
				for (uint32_t j = 0; j < numInstances; j++) {
					fthreads.push_back( server.Request(lrmf[i*numInstances+j]) );
				}
				for (uint32_t j = 0; j < numInstances; j++) {
					fthreads[j]->WaitUntilFinished();
				}
			}

			double end = get_time();
			total += (end - start);

			// Verify
#ifdef XILINX
			server.GetInputHandleFromFPGA(lrmf[0]);
#endif
			lrmf[0]->CopyModel();
			cout << lrmf[0]->RMSE() << endl;
		}

		cout << "Avg time per epoch: " << total/numEpochs << endl;
		cout << "Processing rate: " << (numEpochs*((LRMF*)lrmf[0])->GetDataSize())/total/1e9 << "GB/s" << endl;
#endif
	}

	delete lrmf[0];
	lrmf.clear();

	return 0;
}