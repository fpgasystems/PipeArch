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
	uint32_t numEpochs = 10;
	if (!(argc == 6)) {
		cout << "Usage: ./app <pathToDataset> <Mdim> <numFeatures> <tileSize> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	Mdim = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	tileSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);

	float stepSize = 0.01;
	float lambda = 0;

#ifdef FPGA
	Server server(false, false);
	FPGA_LRMF lrmf(&server, numFeatures);
#else
	LRMF lrmf(numFeatures);
#endif

	if ( strcmp(pathToDataset, "syn") == 0) {
		lrmf.GenerateSyntheticData(Mdim, 2*Mdim);
	}
	else {
		lrmf.ReadNetflixData(pathToDataset, Mdim);
	}
	lrmf.DivideLBIntoTiles(tileSize);

	lrmf.PrintM();
	lrmf.PrintU();

	// lrmf.Optimize(stepSize, lambda, numEpochs);
	lrmf.OptimizeRound(stepSize, lambda, numEpochs);

#ifdef FPGA
	lrmf.RandInitMU();

	lrmf.PrintM();
	lrmf.PrintU();

	lrmf.CreateMemoryLayout();
	lrmf.fOptimizeRound(stepSize, lambda, numEpochs);

	FThread* fthread = server.Request(&lrmf);
	fthread->WaitUntilFinished();

	// Verify
#endif

	return 0;
}