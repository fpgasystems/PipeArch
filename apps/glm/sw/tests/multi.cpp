#include <iostream>
#include "FPGA_ColumnML.h"
#include "Server.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

#define VALUE_TO_INT_SCALER 10

int main(int argc, char* argv[]) {

	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	if (!(argc == 5)) {
		cout << "Usage: ./app <numSamples> <numFeatures> <minibatchSize> <numEpochs>" << endl;
		return 0;
	}
	numSamples = atoi(argv[1]);
	numFeatures = atoi(argv[2]);
	minibatchSize = atoi(argv[3]);
	numEpochs = atoi(argv[4]);

	uint32_t partitionSize = 16000;

	const uint32_t NUM_JOBS = 4;
	Server server(AFU_ACCEL_UUID, true, true);

	FPGA_ColumnML* columnML[NUM_JOBS];
	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		columnML[i] = new FPGA_ColumnML(&server);
	}

	float stepSize = 0.01;
	float lambda = 0;

	ModelType type = linreg;

	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		columnML[i]->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
	}

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML[0]->m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	MemoryFormat format = RowStore;
	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		columnML[i]->CreateMemoryLayout(format, partitionSize);
	}

	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		if (i%2 == 0) {
			if (i == 0) {
				columnML[i]->SGD(type, nullptr, numEpochs, 1, stepSize, lambda, &args);
			}
			columnML[i]->fSGD(type, numEpochs, stepSize, lambda);
		}
		else {
			if (i == 1) {
				columnML[i]->SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
			}
			columnML[i]->fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda);
		}
	}

	FThread* thread[NUM_JOBS];
	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		thread[i] = server.Request(columnML[i]);
	}

	for (uint32_t i = 0; i < NUM_JOBS; i++) {
		thread[i]->WaitUntilFinished();
	}

	for (uint32_t i = 0; i < NUM_JOBS; i++) {

		auto output1 = columnML[i]->CastToFloat('o');
		float* xHistory1 = (float*)(output1 + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML[i]->Loss(type, xHistory1 + e*columnML[i]->m_alignedNumFeatures, lambda, &args);
			cout << "loss " << e << ": " << loss << endl;
		}

		cout << "Response time for thread " << thread[i]->GetId() << ": " << thread[i]->GetResponseTime() << endl;
	}

	return 0;
}