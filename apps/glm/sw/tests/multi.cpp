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

	ColumnML columnML;

	Server server;

	// sleep(1);

	server.Request(&columnML);
	server.Request(&columnML);

	// sleep(1);

	// delete server;


	// GlmMachine glm(AFU_ACCEL_UUID);

	// FPGA_ColumnML columnML;

	// float stepSize = 0.01;
	// float lambda = 0;

	// ModelType type = linreg;

	// columnML.m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);

	// AdditionalArguments args;
	// args.m_firstSample = 0;
	// args.m_numSamples = columnML.m_cstore->m_numSamples;
	// args.m_constantStepSize = true;

	// MemoryFormat format = RowStore;
	// columnML.CreateMemoryLayout(glm, format, partitionSize);

	// columnML.SGD(type, nullptr, numEpochs, 1, stepSize, lambda, &args);
	// columnML.SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);


	// ResultHandle resultHandle[2];
	// resultHandle[0] = glm.fSGD(columnML, type, numEpochs, stepSize, lambda, &args);
	// resultHandle[1] = glm.fSGD_minibatch(columnML, type, numEpochs, minibatchSize, stepSize, lambda, &args);

	// glm.StartProgram(columnML.m_handle, resultHandle[0], 0);
	// glm.StartProgram(columnML.m_handle, resultHandle[1], 1);

	// glm.JoinProgram(resultHandle[0]);
	// glm.JoinProgram(resultHandle[1]);

	// for (uint32_t i = 0; i < 2; i++) {
	// 	auto output = reinterpret_cast<volatile float*>(resultHandle[i].m_outputHandle->c_type());
	// 	float* xHistory = (float*)(output + 16);
	// 	for (uint32_t e = 0; e < numEpochs; e++) {
	// 		float loss = columnML.Loss(type, xHistory + e*columnML.m_alignedNumFeatures, lambda, &args);
	// 		std::cout << "loss " << e << ": " << loss << std::endl;
	// 	}
	// }

	return 0;
}