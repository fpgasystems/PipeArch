#include <iostream>
#include "FPGA_ColumnML.h"
#include "Server.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

#define VALUE_TO_INT_SCALER 10

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	if (!(argc == 6)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <minibatchSize> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	minibatchSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);

	uint32_t partitionSize = 16000;

	Server server(AFU_ACCEL_UUID, true, true);

	FPGA_ColumnML columnML(&server);

	float stepSize;
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


	// Set memory format / decide on SGD or SCD
	MemoryFormat format = RowStore;
	columnML.CreateMemoryLayout(format, partitionSize);


	if (format == RowStore) {
		stepSize = 0.01;
		columnML.SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

		if (minibatchSize == 1) {
			columnML.fSGD(type, numEpochs, stepSize, lambda);
		}
		else {
			columnML.fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda);
		}
	}
	else {
		stepSize = 1;
		columnML.SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);

		columnML.fSCD(type, numEpochs, stepSize, lambda);
	}

	FThread* fthread = server.Request(&columnML);
	fthread->WaitUntilFinished();

	// Verify
	if (format == RowStore) {
		auto output = columnML.CastToFloat('o');
		float* xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML.Loss(type, xHistory + e*columnML.m_alignedNumFeatures, lambda, &args);
			cout << "loss " << e << ": " << loss << endl;
		}
	}
	else {
		float loss = columnML.Loss(type, columnML.GetModelSCD().data(), lambda, &args);
		cout << "loss: " << loss << endl;
	}

	return 0;
}