#include <iostream>
#include "FPGA_ColumnML.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

#define VALUE_TO_INT_SCALER 10

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	if (!(argc == 5)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	numEpochs = atoi(argv[4]);

	uint32_t partitionSize = 16;

	FPGA_ColumnML columnML(AFU_ACCEL_UUID);

	float stepSize = 0.01;
	float lambda = 0;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML.m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		columnML.m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML.m_cstore->NormalizeSamples(ZeroToOne, column);
		columnML.m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		type = logreg;
	}
	columnML.m_cstore->PrintSamples(2);

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML.m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	columnML.SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
	columnML.CopyDataToFPGAMemory(FormatSGD, partitionSize);
	// columnML.fSGD(type, nullptr, numEpochs, stepSize, lambda, &args);
	columnML.fSGD_blocking(type, nullptr, numEpochs, stepSize, lambda, &args);

	// columnML.SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);
	// columnML.CopyDataToFPGAMemory(partitionSize);
}