#include <iostream>
#include "FPGA_ColumnML.h"
#include "GlmMachine.h"

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

	uint32_t partitionSize = 128;

	GlmMachine glm(AFU_ACCEL_UUID);

	FPGA_ColumnML columnML;

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



	// Set memory format / decide on SGD or SCD
	MemoryFormat format = RowStore;
	columnML.CreateMemoryLayout(glm, format, partitionSize);



	ResultHandle resultHandle;
	if (format == RowStore) {
		columnML.SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

		if (minibatchSize == 1) {
			resultHandle = glm.fSGD(columnML, type, numEpochs, stepSize, lambda, &args);
			// resultHandle = glm.fSGD_blocking(columnML, type, numEpochs, stepSize, lambda, &args);
		}
		else {
			resultHandle = glm.fSGD_minibatch(columnML, type, numEpochs, minibatchSize, stepSize, lambda, &args);
		}
	}
	else {
		columnML.SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);

		resultHandle = glm.fSCD(columnML, type, numEpochs, stepSize, lambda, &args);
	}

	glm.JoinProgram(resultHandle);

	// Verify
	if (format == RowStore) {
		auto output = reinterpret_cast<volatile float*>(resultHandle.m_outputHandle->c_type());
		float* xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML.Loss(type, xHistory + e*columnML.m_alignedNumFeatures, lambda, &args);
			std::cout << "loss " << e << ": " << loss << std::endl;
		}
	}
	else {
		std::vector<float> avgModel(columnML.m_alignedNumFeatures);
		for (uint32_t p = 0; p < columnML.m_numPartitions; p++) {
			for (uint32_t j = 0; j < columnML.m_alignedNumFeatures; j++) {
				if (p == 0) {
					avgModel[j] = columnML.m_model[p*columnML.m_alignedNumFeatures + j];
				}
				else {
					avgModel[j] += columnML.m_model[p*columnML.m_alignedNumFeatures + j];
				}
				// cout << "p: " << p << ", avgModel[" << j <<  "]: " << avgModel[j] << endl;
			}
		}
		for (uint32_t j = 0; j < columnML.m_alignedNumFeatures; j++) {
			avgModel[j] /= columnML.m_numPartitions;
		}
		float loss = columnML.Loss(type, avgModel.data(), lambda, &args);
		std::cout << "loss: " << loss << std::endl;
	}

	return 0;
}