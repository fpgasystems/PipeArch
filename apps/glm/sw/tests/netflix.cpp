#include <iostream>
#include "LRMF.h"

using namespace std;

int main(int argc, char* argv[]) {

	char* pathToDataset;
	int Mdim = -1;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	if (!(argc == 5)) {
		cout << "Usage: ./app <pathToDataset> <Mdim> <numFeatures> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	Mdim = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);

	uint32_t tileSize = 512;
	float stepSize = 0.001;
	float lambda = 0.01;

	LRMF lrmf(numFeatures);

	lrmf.ReadNetflixData(pathToDataset, Mdim);

	// lrmf.Optimize(stepSize, lambda, numEpochs);

	// lrmf.OptimizeTransposed(stepSize, lambda, numEpochs);

	lrmf.OptimizeTiled(tileSize, stepSize, lambda, numEpochs);

	return 0;
}