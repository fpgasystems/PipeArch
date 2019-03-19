#include <iostream>
#include "FPGA_ColumnML.h"
#include "GlmMachine.h"
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

	Server server(AFU_ACCEL_UUID);

	FPGA_ColumnML columnML1(&server);
	FPGA_ColumnML columnML2(&server);

	float stepSize = 0.01;
	float lambda = 0;

	ModelType type = linreg;

	columnML1.m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
	columnML2.m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML1.m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	MemoryFormat format = RowStore;
	columnML1.CreateMemoryLayout(format, partitionSize);
	columnML2.CreateMemoryLayout(format, partitionSize);

	columnML1.SGD(type, nullptr, numEpochs, 1, stepSize, lambda, &args);
	columnML2.SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

	columnML1.fSGD(type, numEpochs, stepSize, lambda);
	columnML2.fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda);

	server.Request(&columnML1);
	server.Request(&columnML2);

	columnML1.WaitUntilCompletion();
	columnML2.WaitUntilCompletion();

	auto output1 = columnML1.CastToFloat('o');
	float* xHistory1 = (float*)(output1 + 16);
	for (uint32_t e = 0; e < numEpochs; e++) {
		float loss = columnML1.Loss(type, xHistory1 + e*columnML1.m_alignedNumFeatures, lambda, &args);
		std::cout << "loss " << e << ": " << loss << std::endl;
	}

	auto output2 = columnML2.CastToFloat('o');
	float* xHistory2 = (float*)(output2 + 16);
	for (uint32_t e = 0; e < numEpochs; e++) {
		float loss = columnML2.Loss(type, xHistory2 + e*columnML2.m_alignedNumFeatures, lambda, &args);
		std::cout << "loss " << e << ": " << loss << std::endl;
	}

	return 0;
}