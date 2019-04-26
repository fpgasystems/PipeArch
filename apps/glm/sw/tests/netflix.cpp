#include <iostream>
#include "FPGA_LRMF.h"
#include "Server.h"

using namespace std;

// #define FPGA

int main(int argc, char* argv[]) {

	char* pathToDataset;
	int Mdim = -1;
	uint32_t numFeatures = 0;
	uint32_t tileSize = 0;
	uint32_t numEpochs = 10;
	bool asyncUpdate = false;
	if (!(argc == 7)) {
		cout << "Usage: ./app <pathToDataset> <Mdim> <numFeatures> <tileSize> <asyncUpdate> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	Mdim = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	tileSize = atoi(argv[4]);
	asyncUpdate = (strcmp(argv[5], "y") == 0);
	numEpochs = atoi(argv[6]);

	float stepSize = 0.001;
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

	// lrmf.PrintM();
	// lrmf.PrintU();

	lrmf.RandInitMU();
	lrmf.Optimize(stepSize, lambda, numEpochs);

	lrmf.DivideLBIntoTiles(tileSize);

	lrmf.RandInitMU();
	lrmf.OptimizeRound(stepSize, lambda, numEpochs);

	lrmf.RandInitMU();
	lrmf.OptimizeRoundStale(stepSize, lambda, numEpochs);

#ifdef FPGA
	lrmf.RandInitMU();

	// lrmf.PrintM();
	// lrmf.PrintU();

	lrmf.CreateMemoryLayout();
	lrmf.fOptimizeRound(stepSize, lambda, asyncUpdate, numEpochs);

	FThread* fthread = server.Request(&lrmf);
	fthread->WaitUntilFinished();

	// Verify
	lrmf.CopyModel();
	cout << "FPGA loss: " << lrmf.RMSE() << endl;
#endif

	return 0;
}