#include <iostream>
#include "fsgd.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	if (!(argc == 5)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);

	fsgd fsgd_inst(AFU_ACCEL_UUID);

	float stepSize = 0.01;
	float lambda = 0;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		fsgd_inst.m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		fsgd_inst.m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		fsgd_inst.m_cstore->NormalizeSamples(ZeroToOne, column);
		fsgd_inst.m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		type = logreg;
	}
	fsgd_inst.m_cstore->PrintSamples(2);

	fsgd_inst.CopyDataToFPGAMemory(16384);
	fsgd_inst.fSGD(type, nullptr, numEpochs, stepSize, lambda);
}