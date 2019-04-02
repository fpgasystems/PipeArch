#include <iostream>
#include "LRMF.h"

using namespace std;

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	if (!(argc == 4)) {
		cout << "Usage: ./app <pathToDataset> <numFeatures> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numFeatures = atoi(argv[2]);
	numEpochs = atoi(argv[3]);

	float stepSize = 0.001;
	float lambda = 0.01;

	LRMF lrmf(numFeatures);

	lrmf.ReadNetflixData(pathToDataset);

	lrmf.Optimize(stepSize, lambda, numEpochs);

	return 0;
}