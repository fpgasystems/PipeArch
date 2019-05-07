#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"
#include <thread>

#define VALUE_TO_INT_SCALER 10

#define FPGA

void thread_SGD(
	uint32_t threadId,
	ColumnML* columnML,
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	AdditionalArguments args)
{
	columnML->AVXrowwise_SGD(type, xHistory, numEpochs, minibatchSize, stepSize, lambda, &args);
}

void thread_Loss(
	float* loss,
	ColumnML* columnML,
	ModelType type,
	float* x,
	float lambda,
	AdditionalArguments args)
{
	*loss = columnML->Loss(type, x, lambda, &args);
}

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	uint32_t numClasses = 2;
	uint32_t sw0hw1 = 0;
	if (!(argc == 8)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <minibatchSize> <numEpochs> <numClasses> <sw0hw1>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	minibatchSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);
	numClasses = atoi(argv[6]);
	sw0hw1 = atoi(argv[7]);

	uint32_t partitionSize = 10400;

#ifdef FPGA
	Server server(false, true);
	vector<FPGA_ColumnML*> columnML;
	for (uint32_t c = 0; c < numClasses; c++) {
		columnML.push_back(new FPGA_ColumnML(&server));
	}
#else
	vector<ColumnML*> columnML;
	for (uint32_t c = 0; c < numClasses; c++) {
		columnML.push_back(new ColumnML());
	}
#endif

	float stepSize = 0.001;
	float lambda = 0.0001;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML[0]->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		if (numClasses > 2) {
			columnML[0]->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
			columnML[0]->m_cstore->NormalizeSamples(ZeroToOne, column);
			columnML[0]->m_cstore->PopulateOnehotLabels(numClasses);
			type = logreg;
		}
		else if (numClasses == 2) {
			columnML[0]->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
			columnML[0]->m_cstore->NormalizeSamples(ZeroToOne, column);
			columnML[0]->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
			type = logreg;
		}
		else {
			columnML[0]->m_cstore->LoadLibsvmData(pathToDataset, numSamples, numFeatures, true);
			columnML[0]->m_cstore->PrintSamples(2);
			columnML[0]->m_cstore->NormalizeSamples(ZeroToOne, column);
			type = linreg;
		}
		columnML[0]->m_cstore->PopulateRowSamples();
	}

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML[0]->m_cstore->m_numSamples;
	args.m_constantStepSize = true;
	args.m_useOnehotLabels = false;

	// Set memory format / decide on SGD or SCD
	MemoryFormat format = RowStore;

	if (sw0hw1 == 0) {
		if (numClasses < 2) {
			columnML[0]->AVXrowwise_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
		}
		else {
			vector<float*> xHistories;
			xHistories.resize(numClasses);
			for (uint32_t c = 0; c < numClasses; c++) {
				xHistories[c] = (float*)malloc(numEpochs*columnML[0]->m_cstore->m_numFeatures*sizeof(float));
			}
			vector<thread*> threads(numClasses);

			double start = get_time();
			for (uint32_t c = 0; c < numClasses; c++) {
				args.m_useOnehotLabels = true;
				args.m_class = c;
				threads[c] =
					new thread(
						thread_SGD,
						c,
						columnML[0],
						type,
						xHistories[c],
						numEpochs,
						minibatchSize,
						stepSize,
						lambda,
						args);
			}
			for (uint32_t c = 0; c < numClasses; c++) {
				threads[c]->join();
				delete threads[c];
			}
			double total = get_time() - start;
			cout << "Avg time per epoch: " << total/numEpochs << endl;
			cout << "Processing rate: " << (numClasses*numEpochs*columnML[0]->GetDataSize())/total/1e9 << "GB/s" << endl;

			// Verify
			for (uint32_t e = 0; e < numEpochs; e++) {
				vector<float> losses(numClasses);
				for (uint32_t c = 0; c < numClasses; c++) {
					float* xHistory = xHistories[c];
					args.m_useOnehotLabels = true;
					args.m_class = c;
					threads[c] =
						new thread(
							thread_Loss,
							&losses[c],
							columnML[0],
							type,
							xHistory + e*columnML[0]->m_cstore->m_numFeatures,
							lambda,
							args);
				}
				float loss = 0.0;
				for (uint32_t c = 0; c < numClasses; c++) {
					threads[c]->join();
					delete threads[c];
					loss += losses[c];
				}
				cout << loss/numClasses << endl;
			}
			for (uint32_t c = 0; c < numClasses; c++) {
				free(xHistories[c]);
			}
		}
	}
	else {
#ifdef FPGA
		if (numClasses < 2) {
			columnML[0]->CreateMemoryLayout(format, partitionSize, false);
			if (minibatchSize == 1) {
				columnML[0]->fSGD(type, numEpochs, stepSize, lambda, 0);
			}
			else {
				columnML[0]->fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda, 0);
			}

			double start = get_time();
			FThread* fthread = server.Request(columnML[0]);
			fthread->WaitUntilFinished();
			double total = get_time() - start;
			cout << "Avg time per epoch: " << total/numEpochs << endl;
			cout << "Processing rate: " << (numEpochs*columnML[0]->GetDataSize())/total/1e9 << "GB/s" << endl;

			// Verify
			auto output = iFPGA::CastToFloat(columnML[0]->m_outputHandle);
			float* xHistory = (float*)(output + 16);
			args.m_useOnehotLabels = false;
			args.m_class = 0;
			for (uint32_t e = 0; e < numEpochs; e++) {
				float loss = columnML[0]->Loss(type, xHistory + e*columnML[0]->m_alignedNumFeatures, lambda, &args);
				cout << loss << endl;
			}
		}
		else {
			columnML[0]->CreateMemoryLayout(format, partitionSize, true);
			for (uint32_t c = 1; c < numClasses; c++) {
				columnML[c]->UseCreatedMemoryLayout(columnML[0]);
			}

			for (uint32_t c = 0; c < numClasses; c++) {
				if (minibatchSize == 1) {
					columnML[c]->fSGD(type, numEpochs, stepSize, lambda, c);
				}
				else {
					columnML[c]->fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda, c);
				}
			}

			double start = get_time();
			vector<FThread*> fthreads(numClasses);
			for (uint32_t c = 0; c < numClasses; c++) {
				fthreads[c] = server.Request(columnML[c]);
			}
			for (uint32_t c = 0; c < numClasses; c++) {
				fthreads[c]->WaitUntilFinished();
			}
			double total = get_time() - start;
			cout << "Avg time per epoch: " << total/numEpochs << endl;
			cout << "Processing rate: " << (numClasses*numEpochs*columnML[0]->GetDataSize())/total/1e9 << "GB/s" << endl;

			// Verify
			vector<thread*> threads(numClasses);
			for (uint32_t e = 0; e < numEpochs; e++) {
				vector<float> losses(numClasses);
				for (uint32_t c = 0; c < numClasses; c++) {
					auto output = iFPGA::CastToFloat(columnML[c]->m_outputHandle);
					float* xHistory = (float*)(output + 16);
					args.m_useOnehotLabels = true;
					args.m_class = c;
					threads[c] =
						new thread(
							thread_Loss,
							&losses[c],
							columnML[0],
							type,
							xHistory + e*columnML[0]->m_alignedNumFeatures,
							lambda,
							args);
				}
				float loss = 0.0;
				for (uint32_t c = 0; c < numClasses; c++) {
					threads[c]->join();
					delete threads[c];
					loss += losses[c];
				}
				cout << loss/numClasses << endl;
			}
		}
#endif
	}

	for (uint32_t c = 0; c < numClasses; c++) {
		delete columnML[c];
	}

	return 0;
}