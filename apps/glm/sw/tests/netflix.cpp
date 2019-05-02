#include <iostream>
#include "FPGA_LRMF.h"
#include "Server.h"

using namespace std;

#define FPGA

int main(int argc, char* argv[]) {

	char* pathToDataset;
	int Mdim = -1;
	uint32_t numFeatures = 0;
	uint32_t tileSize = 0;
	bool asyncUpdate = false;
	uint32_t numEpochs = 10;
	uint32_t numInstances = 1;
	if (!(argc == 8)) {
		cout << "Usage: ./app <pathToDataset> <Mdim> <numFeatures> <tileSize> <asyncUpdate> <numEpochs> <numInstances>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	Mdim = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	tileSize = atoi(argv[4]);
	asyncUpdate = (strcmp(argv[5], "y") == 0);
	numEpochs = atoi(argv[6]);
	numInstances = atoi(argv[7]);

	if (numInstances > iFPGA::MAX_NUM_INSTANCES) {
		cout << "numInstances is larger than iFPGA::MAX_NUM_INSTANCES (" << iFPGA::MAX_NUM_INSTANCES << ")" << endl;
		return 1;
	}

	float stepSize = 0.0008;
	float lambda = 0;

#ifdef FPGA
	Server server(false, false);
	vector<FPGA_LRMF*> lrmf;
	lrmf.reserve(numInstances);

	for (uint32_t i = 0; i < numInstances; i++) {
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
		lrmf[0]->GenerateSyntheticData(Mdim, 2*Mdim);
	}
	else {
		lrmf[0]->ReadNetflixData(pathToDataset, Mdim);
	}

	// lrmf[0]->RandInitMU();
	// lrmf[0]->Optimize(stepSize, lambda, numEpochs);

	lrmf[0]->DivideLBIntoTiles(tileSize);

	// lrmf[0]->RandInitMU();
	// lrmf[0]->OptimizeRound(stepSize, lambda, numEpochs);

	lrmf[0]->RandInitMU();
	lrmf[0]->OptimizeRoundStale(stepSize, lambda, numEpochs);

#ifdef FPGA
	lrmf[0]->RandInitMU();

	lrmf[0]->CreateMemoryLayout();
	for (uint32_t i = 1; i < numInstances; i++) {
		lrmf[i]->UseCreatedMemoryLayout(lrmf[0]);
	}

	vector<uint32_t> MtilesPermutation(numInstances*numInstances);
	vector<uint32_t> UtilesPermulation(numInstances*numInstances);
	for (uint32_t i = 0; i < numInstances; i++) {
		for (uint32_t j = 0; j < numInstances; j++) {
			MtilesPermutation[i*numInstances+j] = j;
			UtilesPermulation[i*numInstances+j] = (i+j)%numInstances;
		}
	}

	vector<uint32_t> MtileToStart(numInstances*numInstances);
	vector<uint32_t> numTilesM(numInstances*numInstances);
	vector<uint32_t> UtileToStart(numInstances*numInstances);
	vector<uint32_t> numTilesU(numInstances*numInstances);
	uint32_t MtilesPerInstance = lrmf[0]->GetNumTilesM()/numInstances;
	uint32_t UtilesPerInstance = lrmf[0]->GetNumTilesU()/numInstances;

	for (uint32_t i = 0; i < numInstances; i++) {
		for (uint32_t j = 0; j < numInstances; j++) {
			MtileToStart[i*numInstances+j] = MtilesPermutation[i*numInstances+j]*MtilesPerInstance;
			if (MtilesPermutation[i*numInstances+j] == numInstances-1)
				numTilesM[i*numInstances+j] = lrmf[0]->GetNumTilesM() - MtileToStart[i*numInstances+j];
			else
				numTilesM[i*numInstances+j] = MtilesPerInstance;

			UtileToStart[i*numInstances+j] = UtilesPermulation[i*numInstances+j]*UtilesPerInstance;
			if (UtilesPermulation[i*numInstances+j] == numInstances-1)
				numTilesU[i*numInstances+j] = lrmf[0]->GetNumTilesU() - UtileToStart[i*numInstances+j];
			else
				numTilesU[i*numInstances+j] = UtilesPerInstance;

			cout << "numTilesM " << i << " " << j << ": " << numTilesM[i*numInstances+j] << endl;
			cout << "numTilesU " << i << " " << j << ": " << numTilesU[i*numInstances+j] << endl;
		}
	}

	double start = get_time();

	for (uint32_t i = 0; i < numInstances; i++) {
		vector<FThread*> fthreads;
		for (uint32_t j = 0; j < numInstances; j++) {
			lrmf[j]->fOptimizeRound(
				MtileToStart[i*numInstances+j], numTilesM[i*numInstances+j],
				UtileToStart[i*numInstances+j], numTilesU[i*numInstances+j],
				stepSize, lambda, asyncUpdate, numEpochs);
			fthreads.push_back( server.Request(lrmf[j]) );
		}
		for (uint32_t j = 0; j < numInstances; j++) {
			fthreads[j]->WaitUntilFinished();
		}
	}

	double total = get_time() - start;
	cout << "Avg time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << (numEpochs*lrmf[0]->GetDataSize())/total/1e9 << "GB/s" << endl;

	// Verify
	lrmf[0]->CopyModel();
	cout << "FPGA loss: " << lrmf[0]->RMSE() << endl;
#endif

	for (uint32_t i = 0; i < lrmf.size(); i++) {
		delete lrmf[i];
	}
	lrmf.clear();

	return 0;
}